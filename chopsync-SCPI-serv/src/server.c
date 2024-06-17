/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: jun 17 2024                   ***
 ***                                            ***
 **************************************************/ 

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
//#include <stdbool.h>

#define PORT    8888
#define MAXMSG  512

#define READ  1
#define WRITE 0

#define ERRS "ERR"
#define OKS  "OK"

#define MAXREG 12

/***  protos  ***/

void   upstring(char *s);
void   trimstring(char* s);
void   parse(char *buf, char *ans, size_t maxlen);
int    read_from_client(int filedes);
int    main(int argc, char *const argv[]);


/***  implementation  ***/

void upstring(char *s)
  {
  char *p;
  for(p=s; p*; p++)
    *p=toupper((unsigned char) *p);
  }


//-------------------------------------------------------------------

void trimstring(char* s)
  {
  char   *begin, *end;
  char   wbuf[MAXMSG+1];
  size_t len;

  len=strlen(s);
  if(!len)
    return;
  
  for(end=s+len-1; end >=s && isspace(*end); end--)
    ;
  for(begin=s; begin <=end && isspace(*begin); begin++)
    ;
  
  len=end-begin;
  // must use memcpy because I don't have proper 0-termination yet
  memcpy(wbuf, begin, len);
  wbuf[len+1]=0;
  // I can use strcpy as now I have proper 0-termination
  (void) strcpy(s, wbuf);
  return len;
  }


//-------------------------------------------------------------------

void parse(char *buf, char *ans, size_t maxlen)
  {
  char *p;
  int rw, n;
  unsigned int reg, val;

  trimstring(buf);
  upstring(buf);

  p=strtok(buf,'?');
  if(p!=NULL)
    {
    rw=READ;
    }
    {
    rw=WRITE;
    p=strtok(buf,' ');
    }

  if( (strcmp(p,"REG")=0) || (strcmp(p,"REGISTER")=0))
    {
    p=strtok(NULL,' ');
    if(p!=NULL)
      {
      n=sscanf(p,"%x",&reg);
      if(n==0 || reg>MAXREG)
        {
        snprintf(ans, maxlen, "%s: no such register\n", ERRS);    
        }
      else
        {
        if(rw==WRITE)
          {
          p=strtok(NULL,' ');
          if(p!=NULL)
            {
            n=sscanf(p,"%x",&val);
            if(n==0)
              {
              snprintf(ans, maxlen, "%s: invalid value to write into register\n", ERRS);
              }
            else
              {
              // WRITE REGISTER
              snprintf(ans, maxlen, "%s: write 0x%08X into register 0x%X\n", OKS, val, reg);
              }
            }
          else
            snprintf(ans, maxlen, "%s: missing value to write into register\n", ERRS);
          }
        else
          {
          // READ REGISTER
          snprintf(ans, maxlen, "%s: read register 0x%X\n", OKS, reg);
          }
        }
      }
    else
      {
      snprintf(ans, maxlen, "%s: missing register number\n", ERRS);
      }
    }
  else if(strcmp(p,"*IDN")=0)
    {
    
    }
  else
    {
    snprintf(ans, maxlen, "%s: no such command\n", ERRS);
    }
  }

//-------------------------------------------------------------------

int read_from_client(int filedes)
  {
  char buffer[MAXMSG+1];    // "+1" to add zero-terminator
  char echomsg[2*MAXMSG]="OK:";
  char answer[MAXMSG+1];
  int  nbytes;
  
  nbytes = read(filedes, buffer, MAXMSG);
  if(nbytes < 0)
    {
    // read error
    perror("read");
    exit(EXIT_FAILURE);
    }
  else if(nbytes == 0)
    {
    // end of file
    return -1;
    }
  else
    {
    // data read
    buffer[nbytes]=0;    // add string zero terminator
    fprintf(stderr, "Incoming msg: '%s'\n", buffer);
    parse(buffer, answer, MAXMSG);
    nbytes = write(filedes, answer, strlen(answer));
    return 0;
    }
  }


//-------------------------------------------------------------------

int main(int argc, char *const argv[])
  {
  int sock, maxfd, opt = 1, i, nready;
  fd_set active_fd_set, read_fd_set;
  struct sockaddr_in clientname;
  size_t size;
  struct sockaddr_in name;

  fprintf(stderr,"Starting server\n");

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock < 0)
    {
    perror("socket");
    exit(EXIT_FAILURE);
    }

  if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int)) )
    {
    perror("setsockopt()");
    exit(EXIT_FAILURE);
    }

  name.sin_family = AF_INET;
  name.sin_port = htons(PORT);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if( bind( sock, (struct sockaddr *) &name, sizeof(name)) < 0)
    {
    perror("bind");
    exit(EXIT_FAILURE);
    }

  if(listen(sock, 1) < 0)
    {
    perror("listen");
    exit(EXIT_FAILURE);
    }
  
  // initialize the set of active sockets; 
  // see man 2 select for FD_functions documentation
  FD_ZERO(&active_fd_set);
  FD_SET(sock, &active_fd_set);

  maxfd = sock;

  while(1)
    {
    // block until input arrives on one or more active sockets
    fprintf(stderr,"Listening\n");
    read_fd_set = active_fd_set;
    nready=select(maxfd+1, &read_fd_set, NULL, NULL, NULL);
    if(nready<0)
      {
      perror("select");
      exit(EXIT_FAILURE);
      }

    // service all the sockets with input pending
    for(i=0; i<=maxfd && nready>0; i++)
      {
      if(FD_ISSET(i, &read_fd_set))
        {
        nready--;
        if(i == sock)
          {
          // new connection request on original socket
          int newfd;
          size = sizeof(clientname);
          newfd = accept(sock,
                        (struct sockaddr *) &clientname,
                        &size);
          if(newfd < 0)
            {
            perror("accept");
            exit(EXIT_FAILURE);
            }
          fprintf(stderr,
                 "Server: new connection from host %s, port %hd\n",
                 inet_ntoa(clientname.sin_addr),
                 ntohs(clientname.sin_port));
          FD_SET(newfd, &active_fd_set);
          if(newfd>maxfd)
            {
            maxfd=newfd;
            }
          }    // if new connection
          else
          {
          // data arriving on an already-connected socket
          fprintf(stderr,"Incoming data\n");
          if(read_from_client(i) < 0)
            {
            fprintf(stderr,"Closing connection\n");
            close(i);
            FD_CLR(i, &active_fd_set);
            // I don't update maxfd; I should loop on the fd set to find the new maximum: not worth
            }
          }    // if data from already-connected client
        }    // if input pending
      }    // loop on FD set
    }
  }
