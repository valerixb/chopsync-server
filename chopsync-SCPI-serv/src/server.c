/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: jun 18 2024                   ***
 ***                                            ***
 **************************************************/ 

#include "server.h"

/***  globals  ***/
uint32_t *regbank;

/***  protos  ***/

int          memorymap(void);
void         writereg(unsigned int reg, unsigned int val);
unsigned int readreg(unsigned int reg);
void         upstring(char *s);
void         trimstring(char* s);
void         parseREG(char *ans, size_t maxlen, int rw);
void         parseIDN(char *ans, size_t maxlen, int rw);
void         printHelp(int filedes);
void         parse(char *buf, char *ans, size_t maxlen, int filedes);
int          read_from_client(int filedes);
int          main(int argc, char *const argv[]);


/***  implementation  ***/

int memorymap(void)
  {
  int fd;
  
  if((fd = open("/dev/mem", O_RDWR | O_SYNC)) != -1)
    {
    regbank = (uint32_t *)mmap(NULL, REGBANK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, REGBANK_BASE);
    if(regbank==MAP_FAILED)
      return -1;
    // file descriptor can be closed without invalidating the mapping
    close(fd);
    return 0;
    }
  else
    return -1;
  }


//-------------------------------------------------------------------

void writereg(unsigned int reg, unsigned int val)
  {
  regbank[reg]=val;
  }


//-------------------------------------------------------------------

unsigned int readreg(unsigned int reg)
  {
  return regbank[reg];
  }


//-------------------------------------------------------------------

void upstring(char *s)
  {
  char *p;
  for(p=s; *p; p++)
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
  
  len=end-begin+1;
  // must use memcpy because I don't have proper 0-termination yet
  memcpy(wbuf, begin, len);
  wbuf[len]=0;
  // I can use strcpy as now I have proper 0-termination
  (void) strcpy(s, wbuf);
  }


//-------------------------------------------------------------------

void parseREG(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n;
  unsigned int reg, val;
  
  // next in line is register address [0..MAXREG]
  p=strtok(NULL," ");
  if(p!=NULL)
    {
    // try to read a hex value; remember the string is uppercase
    n=sscanf(p,"0X%x",&reg);
    // if failed, try to read a decimal value
    if(n==0)
      n=sscanf(p,"%u",&reg);
    
    if(n==0 || reg>MAXREG)
      {
      snprintf(ans, maxlen, "%s: no such register\n", ERRS);    
      }
    else
      {
      // we have a valid register address
      if(rw==WRITE)
        {
        // next in line is the value to write into the register
        p=strtok(NULL," ");
        if(p!=NULL)
          {
          // try to read a hex value; remember the string is uppercase
          n=sscanf(p,"0X%x",&val);
          // if failed, try to read a decimal value
          if(n==0)
            n=sscanf(p,"%u",&val);
          
          if(n==0)
            {
            snprintf(ans, maxlen, "%s: invalid value to write into register 0x%X\n", ERRS, reg);
            }
          else
            {
            // WRITE REGISTER
            writereg(reg,val);
            snprintf(ans, maxlen, "%s: write 0x%08X into register 0x%X\n", OKS, val, reg);
            }
          }
        else
          snprintf(ans, maxlen, "%s: missing value to write into register\n", ERRS);
        }
      else
        {
        // READ REGISTER
        snprintf(ans, maxlen, "%s: read register 0x%X: 0x%08X\n", OKS, reg, readreg(reg));
        }
      }
    }
  else
    {
    snprintf(ans, maxlen, "%s: missing register number\n", ERRS);
    }
  }

//-------------------------------------------------------------------

void parseIDN(char *ans, size_t maxlen, int rw)
  {

  }


//-------------------------------------------------------------------

void printHelp(int filedes)
  {
  
  }


//-------------------------------------------------------------------

void parse(char *buf, char *ans, size_t maxlen, int filedes)
  {
  char *p;
  int rw;

  trimstring(buf);
  upstring(buf);

  // is this a READ or WRITE operation?
  // note: cannot use strtok because it modifies the input string
  p=strchr(buf,'?');
  if(p!=NULL)
    {
    rw=READ;
    p=strtok(buf,"?");
    }
  else
    {
    rw=WRITE;
    p=strtok(buf," ");
    }

  // serve the right command
  if( (strcmp(p,"REG")==0) || (strcmp(p,"REGISTER")==0))
    parseREG(ans, maxlen, rw);
  else if(strcmp(p,"*IDN")==0)
    parseIDN(ans, maxlen, rw);
  else if(strcmp(p,"HELP")==0)
    {
    printHelp(filedes);
    *ans=0;
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
    parse(buffer, answer, MAXMSG, filedes);
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

  // map register bank into user space
  if(memorymap()!=0)
    {
    fprintf(stderr,"Can't map Register Bank - aborted\n");
    return -1;
    }

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
