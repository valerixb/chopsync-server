/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: jun 19 2024                   ***
 ***                                            ***
 **************************************************/ 

#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
//#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>

#define PORT    8888
#define MAXMSG  512

#define READ  1
#define WRITE 0
#define ERRS "ERR"
#define OKS  "OK"
#define MAXREG 12

#define REGBANK_BASE 0xA0000000
#define REGBANK_SIZE 256

#define PRODUCT_FNAME "/etc/petalinux/product"
#define VERSION_FNAME "/etc/petalinux/version"

#define SYNCH_RESET_MASK 0x0004

#define MAX_SETPOINT_CNTS 65535
#define PHSETPOINT_MASK 0x0001FFFF
#define PRESCALER_MASK 0x000003FF
#define MAX_PRESCALER 1024
#define BUNCHMARKER 9
#define CHOPPER 10
#define TRIGOUT_MASK 0x000003FF
#define MAX_G 65535
#define UNWTHR_MASK 0x0001FFFF
#define MAX_UNWTHR_CNTS 65535
#define POW_2_12 4096.

#endif