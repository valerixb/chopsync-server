/**************************************************
 ***                                            ***
 ***  chopsync TCP server (SCPI)                ***
 ***                                            ***
 ***  latest rev: jun 3 2024                    ***
 ***                                            ***
 **************************************************/ 

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT    8888
#define MAXMSG  512

/***  protos  ***/

int read_from_client(int filedes);
int main(int argc, char *const argv[]);


/***  implementation  ***/

int read_from_client(int filedes)
  {
  char buffer[MAXMSG+1];    // "+1" to add zero-terminator
  char echomsg[2*MAXMSG]="OK:";
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
    // echo
    strcat(echomsg, buffer);
    nbytes = write(filedes, echomsg, strlen(echomsg));
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
