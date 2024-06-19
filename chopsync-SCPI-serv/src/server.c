/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: jun 19 2024                   ***
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
void         parseSYNCHRONIZER(char *ans, size_t maxlen, int rw);
void         parseRST(char *ans, size_t maxlen, int rw);
void         parsePHSETP(char *ans, size_t maxlen, int rw);
void         parsePRESCALER(char *ans, size_t maxlen, int rw, int regnum);
void         parseTRIGOUTPH(char *ans, size_t maxlen, int rw);
void         parseUNWTHR(char *ans, size_t maxlen, int rw);
void         parseSIGGENDFTW(char *ans, size_t maxlen, int rw);
void         parseGAIN(char *ans, size_t maxlen, int rw);
void         printHelp(int filedes);
void         parse(char *buf, char *ans, size_t maxlen, int filedes);
void         sendback(int filedes, char *s);
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
        snprintf(ans, maxlen, "%s: 0x%08X in register 0x%X\n", OKS, readreg(reg), reg);
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
  FILE *fd;
  char prod[MAXMSG+1], ver[MAXMSG+1];
  char *s;

  // firmware name
  fd = fopen(PRODUCT_FNAME, "r");
  if(fd!=NULL)
    {
    s=fgets(prod, MAXMSG, fd);
    fclose(fd);
    if(s==NULL)
      strcpy(prod,"Unknown Firmware");
    }
  else
    strcpy(prod,"Unknown Firmware");
  
  // firmware version
  fd = fopen(VERSION_FNAME, "r");
  if(fd!=NULL)
    {
    s=fgets(ver, MAXMSG, fd);
    fclose(fd);
    if(s==NULL)
      strcpy(ver,"Unknown");
    }
  else
    strcpy(ver,"Unknown");
  
  trimstring(prod);
  trimstring(ver);
  snprintf(ans, maxlen, "%s - version %s\n", prod, ver);
  }


//-------------------------------------------------------------------

void parseSTB(char *ans, size_t maxlen, int rw)
  {
   snprintf(ans, maxlen, 
            "%s: 0x%03X is the combined status word\n", OKS, 
            ((readreg(1)&0x00FF)<<8 | (readreg(0)&0x00FF))
           );
  }


//-------------------------------------------------------------------

void parseSYNCHRONIZER(char *ans, size_t maxlen, int rw)
  {
  char *p;
  
  if(rw==READ)
    {
    // read synchronizer on/off state
    if((readreg(1) & SYNCH_RESET_MASK) == 0)
      snprintf(ans, maxlen, "%s: ON\n", OKS);
    else
      snprintf(ans, maxlen, "%s: OFF\n", OKS);
    }
  else
    {
    // turn synchronizer on/off
    
    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ON")==0)
        {
        writereg(1,readreg(1) & ~SYNCH_RESET_MASK);
        snprintf(ans, maxlen, "%s: SYNCHRONIZER is now ON\n", OKS);
        }
      else if(strcmp(p,"OFF")==0)
        {
        writereg(1,readreg(1) | SYNCH_RESET_MASK);
        snprintf(ans, maxlen, "%s: SYNCHRONIZER is now OFF\n", OKS);
        }
      else
        snprintf(ans, maxlen, "%s: use ON/OFF with SYNCHRONIZER command\n", ERRS);
      }
    else
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseRST(char *ans, size_t maxlen, int rw)
  {
  writereg(1,readreg(1) | SYNCH_RESET_MASK);
  snprintf(ans, maxlen, "%s: SYNCHRONIZER is now OFF\n", OKS);        
  }


//-------------------------------------------------------------------

void parsePHSETP(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n;
  
  if(rw==READ)
    {
    // read phase setpoint and convert it to ns
    n=(int)readreg(3);
    snprintf(ans, maxlen, "%s: %d ns\n", OKS, n*8);
    }
  else
    {
    // set new phase setpoint
    
    // next in line is the desired setpoint in ns
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      n=(int)strtol(p, NULL, 10);
      if(errno!=0 && n==0)
        snprintf(ans, maxlen, "%s: invalid setpoint value\n", ERRS);
      else
        {
        n=round(n/8.);
        if(n>MAX_SETPOINT_CNTS)
          n=MAX_SETPOINT_CNTS;
        if(n<-MAX_SETPOINT_CNTS)
          n=-MAX_SETPOINT_CNTS;
        writereg(3,((unsigned int)n) & PHSETPOINT_MASK);
        snprintf(ans, maxlen, "%s: new setpoint is %d ns\n", OKS, n*8);
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing setpoint specification\n", ERRS);
    }

  }


//-------------------------------------------------------------------

// choosing regnum in the parameters lets you choose to change 
// the bunchmarker or the chopper prescaler

void parsePRESCALER(char *ans, size_t maxlen, int rw, int regnum)
  {
  char *p;
  int n;
  
  if(rw==READ)
    {
    // read prescaler
    n=(int)(readreg(regnum) & PRESCALER_MASK);
    snprintf(ans, maxlen, "%s: %d\n", OKS, n);
    }
  else
    {
    // set new prescaler value
    
    // next in line is the desired scaler value
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      n=(int)strtol(p, NULL, 10);
      if(errno!=0 && n==0)
        snprintf(ans, maxlen, "%s: invalid prescaler value\n", ERRS);
      else
        {
        if(n>MAX_PRESCALER)
          n=MAX_PRESCALER;
        if(n<1)
          n=1;
        writereg(regnum,(unsigned int)n);
        snprintf(ans, maxlen, "%s: new prescaler is %d\n", OKS, n);
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing prescaler value\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseTRIGOUTPH(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n, presc;
  
  if(rw==READ)
    {
    // read TRIGOUT phase value
    n=(int)(readreg(12) & TRIGOUT_MASK);
    snprintf(ans, maxlen, "%s: %d\n", OKS, n);
    }
  else
    {
    // set new TRIGOUT phase
    
    // next in line is the desired value
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      n=(int)strtol(p, NULL, 10);
      if(errno!=0 && n==0)
        snprintf(ans, maxlen, "%s: invalid value\n", ERRS);
      else
        {
        // TRIGOUT phase must be in range [1..bunchmarker_prescaler]
        presc=(int)(readreg(BUNCHMARKER) & PRESCALER_MASK);
        if(n>presc)
          n=presc;
        if(n<1)
          n=1;
        writereg(12,((unsigned int)n) & TRIGOUT_MASK);
        snprintf(ans, maxlen, "%s: new TRIGOUT phase is %d\n", OKS, n);
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing TRIGOUT phase value\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseUNWTHR(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n, presc;
  
  if(rw==READ)
    {
    // read unwrapper threshold
    n=(int)(readreg(2) & UNWTHR_MASK);
    snprintf(ans, maxlen, "%s: %d\n", OKS, n);
    }
  else
    {
    // set new unwrapper threshold
    
    // next in line is the desired value
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      n=(int)strtol(p, NULL, 10);
      if(errno!=0 && n==0)
        snprintf(ans, maxlen, "%s: invalid value\n", ERRS);
      else
        {
        if(n>MAX_UNWTHR_CNTS)
          n=MAX_UNWTHR_CNTS;
        if(n<0)
          n=0;
        writereg(2,((unsigned int)n) & UNWTHR_MASK);
        snprintf(ans, maxlen, "%s: new unwrapper reset threshold is %d\n", OKS, n);
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing threshold value\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseSIGGENDFTW(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n;
  float df;
  
  if(rw==READ)
    {
    // read deltaFTW of diagnostic bunchmarker generator
    // scale is 1 Hz = 2199 counts
    n=(int)readreg(4);
    df=n/(2199.);
    snprintf(ans, maxlen, "%s: %f\n", OKS, df);
    }
  else
    {
    // set new deltaFTW
    
    // next in line is the desired value
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      df=strtof(p, NULL);
      if(errno!=0 && n==0)
        snprintf(ans, maxlen, "%s: invalid frequency\n", ERRS);
      else
        {
        // convert to sfix_32.0
        // scale is 1 Hz = 2199 counts
        n=round(df*2119.);
        // no coercing; we use 32 bit
        writereg(4,(unsigned int)n);
        snprintf(ans, maxlen, "%s: new frequency is 3'123'437.5 %+f Hz\n", OKS, n/(2199.));
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing frequency specification\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseGAIN(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n;
  float g;
  
  if(rw==READ)
    {
    // read gain and convert it from ufix_16.12
    n=(int)(readreg(11) & GAIN_MASK);
    g=n/POW_2_12;
    snprintf(ans, maxlen, "%s: %f\n", OKS, g);
    }
  else
    {
    // set new gain
    
    // next in line is the desired value
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      g=strtof(p, NULL);
      if(errno!=0 && n==0)
        snprintf(ans, maxlen, "%s: invalid gain\n", ERRS);
      else
        {
        // convert to ufix_16.12
        n=round(g*POW_2_12);
        if(n>MAX_G)
          n=MAX_G;
        if(n<1)
          n=1;
        writereg(11,((unsigned int)n) & GAIN_MASK);
        snprintf(ans, maxlen, "%s: new gain is %f\n", OKS, n/POW_2_12);
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing gain specification\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void printHelp(int filedes)
  {
  sendback(filedes,"Chopsync SCPI server commands\n\n");
  sendback(filedes,"Server support multiple concurrent clients\n");
  sendback(filedes,"Server is case insensitive\n");
  sendback(filedes,"Numbers can be decimal or hex, with the 0x prefix\n");
  sendback(filedes,"Server answers with OK or ERR, a colon and a descriptive message\n");
  sendback(filedes,"Send CTRL-D to close the connection\n\n");
  sendback(filedes,"Command list:\n\n");
  sendback(filedes,"REGister <reg> <value>        : write <value> into register <reg>\n");
  sendback(filedes,"REGister? <reg>               : read content of register <reg>\n");
  sendback(filedes,"*IDN?                         : print firmware name and version\n");
  sendback(filedes,"*STB?                         : combined status word = lower 8 LSBs of reg#1 (<<8) + lower 8 LSBs of reg#0\n");
  sendback(filedes,"SYNCHronizer {ON|OFF}         : turn synchronizer on or off\n");
  sendback(filedes,"SYNCHronizer?                 : query synchronizer state; answer is either ON or OFF\n");
  sendback(filedes,"*RST                          : turn off synchronizer; equivalent to SYNCH OFF\n");
  sendback(filedes,"PHSETPOINT_NS <value>         : set phase setpoint to <value> ns\n");
  sendback(filedes,"PHSETPOINT_NS?                : query current phase setpoint, expressed in ns\n");
  sendback(filedes,"BUNCHMARKER_PRESCALER <value> : set prescaler for bunchmarker\n");
  sendback(filedes,"BUNCHMARKER_PRESCALER?        : query the value of the bunchmarker prescaler\n");
  sendback(filedes,"CHOPPER_PRESCALER <value>     : set prescaler for chopper photodiode\n");
  sendback(filedes,"CHOPPER_PRESCALER?            : query the value of the chopper photodiode prescaler\n");
  sendback(filedes,"TRIGOUT_PH <value>            : set phase for TRIGOUT signal; range [1 to BUNCHMARKER_PRESCALE]\n");
  sendback(filedes,"TRIGOUT_PH?                   : query the value of the TRIGOUT signal phase\n");
  sendback(filedes,"UNW_THR <value>               : [advanced - be careful] set threshold for unwrapper reset\n");
  sendback(filedes,"UNW_THR?                      : query the threshold for unwrapper reset\n");
  sendback(filedes,"SIGGEN_DF_HZ <value>          : [advanced - be careful] delta frequency for diagnostic bunch marker generator\n");
  sendback(filedes,"                                Frequency will be 3'123'437.5 + <value> Hz; <value> can be negative\n");
  sendback(filedes,"SIGGEN_DF_HZ?                 : query the diagnostic bunch marker generator delta frequency\n");
  sendback(filedes,"Gain <value>                  : [advanced - be careful] set loop gain (conservative=4; high performance=default=6)\n");
  sendback(filedes,"Gain?                         : query loop gain\n");
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
  else if(strcmp(p,"*STB")==0)
    parseSTB(ans, maxlen, rw);
  else if( (strcmp(p,"SYNCH")==0) || (strcmp(p,"SYNCHRONIZER")==0))
    parseSYNCHRONIZER(ans, maxlen, rw);
  else if(strcmp(p,"*RST")==0)
    parseRST(ans, maxlen, rw);
  else if(strcmp(p,"PHSETPOINT_NS")==0)
    parsePHSETP(ans, maxlen, rw);
  else if(strcmp(p,"BUNCHMARKER_PRESCALER")==0)
    parsePRESCALER(ans, maxlen, rw, BUNCHMARKER);
  else if(strcmp(p,"CHOPPER_PRESCALER")==0)
    parsePRESCALER(ans, maxlen, rw, CHOPPER);
  else if(strcmp(p,"TRIGOUT_PH")==0)
    parseTRIGOUTPH(ans, maxlen, rw);
  else if(strcmp(p,"UNW_THR")==0)
    parseUNWTHR(ans, maxlen, rw);
  else if(strcmp(p,"SIGGEN_DF_HZ")==0)
    parseSIGGENDFTW(ans, maxlen, rw);
  else if( (strcmp(p,"G")==0) || (strcmp(p,"GAIN")==0))
    parseGAIN(ans, maxlen, rw);
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

void sendback(int filedes, char *s)
  {
  (void)write(filedes, s, strlen(s));
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
    //fprintf(stderr, "Incoming msg: '%s'\n", buffer);
    parse(buffer, answer, MAXMSG, filedes);
    sendback(filedes, answer);
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
    //fprintf(stderr,"Listening\n");
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
          //fprintf(stderr,"Incoming data\n");
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
