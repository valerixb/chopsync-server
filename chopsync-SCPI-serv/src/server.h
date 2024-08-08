/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: aug  8 2024                   ***
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
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include "can.h"


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

#define FREQUENCY 0x0001
#define PHASE 0x0002
#define STICKYLOL_MASK 0x0020

#define UNWRAPPER_MASK 0x0001
#define UNWRESET_MASK 0x0002
#define SYNCH_RESET_MASK 0x0004
#define LOL_RESET_MASK 0x0008

#define MAX_SETPOINT_CNTS 65535
#define PHSETPOINT_MASK 0x0001FFFF
#define PHSETPOINT_SIGN 0x00010000
#define PRESCALER_MASK 0x000003FF
#define MAX_PRESCALER 1024

#define BUNCHMARKER_FREQ_REG 7
#define CHOPPER_FREQ_REG 8
#define BUNCHMARKER_PSCALER_REG 9
#define CHOPPER_PSCALER_REG 10

#define TRIGOUT_MASK 0x000003FF
#define GAIN_MASK 0x0000FFFF
#define MAX_G 65535
#define UNWTHR_MASK 0x0001FFFF
#define MAX_UNWTHR_CNTS 65535

#define MECOSCMD_MASK 0x003FFFFF
#define MECOSCMD_SIGN 0x00200000

#define PHERR_MASK 0x00FFFFFF
#define PHERR_SIGN 0x00800000

#define POW_2_12 4096.
#define POW_2_7 128.

#define MECOS_MAX_SPEED    1000


/***  protos  ***/

int          memorymap(void);
void         writereg(unsigned int reg, unsigned int val);
unsigned int readreg(unsigned int reg);
void         upstring(char *s);
void         trimstring(char* s);
void         parseREG(char *ans, size_t maxlen, int rw);
void         parseIDN(char *ans, size_t maxlen);
void         parseSTB(char *ans, size_t maxlen);
void         parseSYNCHRONIZER(char *ans, size_t maxlen, int rw);
void         parseRST(char *ans, size_t maxlen);
void         parsePHSETP(char *ans, size_t maxlen, int rw);
void         parsePRESCALER(char *ans, size_t maxlen, int rw, int regnum);
void         parseUNWRAP(char *ans, size_t maxlen, int rw);
void         parseUNWRES(char *ans, size_t maxlen, int rw);
void         parseTRIGOUTPH(char *ans, size_t maxlen, int rw);
void         parseUNWTHR(char *ans, size_t maxlen, int rw);
void         parseSIGGENDFTW(char *ans, size_t maxlen, int rw);
void         parseGAIN(char *ans, size_t maxlen, int rw);
void         parseLOCK(char *ans, size_t maxlen, int rw, unsigned int mask);
void         parsePHERR(char *ans, size_t maxlen, int rw);
void         parseMECOSCMD(char *ans, size_t maxlen, int rw);
void         parseFREQ(char *ans, size_t maxlen, int rw, int regnum);
void         parseLOL(char *ans, size_t maxlen, int rw);
void         parseMECOS_HZ_SETP(char *ans, size_t maxlen, int rw);
void         parseMECOS_HZ_ACT(char *ans, size_t maxlen, int rw);
void         parseMECOS_LIFTUP(char *ans, size_t maxlen, int rw);
void         parseMECOS_ROTATION(char *ans, size_t maxlen, int rw);
void         parseMECOS_FAULT(char *ans, size_t maxlen, int rw);
void         parseMECOS_STABLE(char *ans, size_t maxlen, int rw);
void         printHelp(int filedes);
void         parse(char *buf, char *ans, size_t maxlen, int filedes);
void         sendback(int filedes, char *s);
int          read_from_client(int filedes);
//int          main(int argc, char *const argv[]);
int          main(void);

#endif