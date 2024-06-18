/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: jun 18 2024                   ***
 ***                                            ***
 **************************************************/ 

#ifndef SERVER_H
#define SERVER_H

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


#endif