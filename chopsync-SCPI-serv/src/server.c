/**************************************************
 ***                                            ***
 ***  chopsync TCP server (kinda SCPI)          ***
 ***                                            ***
 ***  latest rev: aug  8 2024                   ***
 ***                                            ***
 **************************************************/ 

#include "server.h"

/***  globals  ***/
uint32_t *regbank;
int      can_present;

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

void parseIDN(char *ans, size_t maxlen)
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

void parseSTB(char *ans, size_t maxlen)
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
    snprintf(ans, maxlen, "%s: %s\n", OKS, ((readreg(1) & SYNCH_RESET_MASK) == 0)?"ON":"OFF");
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

void parseRST(char *ans, size_t maxlen)
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
    n=(int)(readreg(3) & PHSETPOINT_MASK);
    // sign extension
    n=(n ^ PHSETPOINT_SIGN)-PHSETPOINT_SIGN;
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

// choosing regnum in the parameters lets you choose to read 
// the bunchmarker or the chopper frequency

void parseFREQ(char *ans, size_t maxlen, int rw, int regnum)
  {
  int n;
  
  if(rw==READ)
    {
    // read frequency
    n=(int)readreg(regnum);
    snprintf(ans, maxlen, "%s: %d Hz\n", OKS, n);
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);

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
        presc=(int)(readreg(BUNCHMARKER_PSCALER_REG) & PRESCALER_MASK);
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

void parseUNWRAP(char *ans, size_t maxlen, int rw)
  {
  char *p;
  
  if(rw==READ)
    {
    // read unwrapper on/off state
    snprintf(ans, maxlen, "%s: %s\n", OKS, ((readreg(1) & UNWRAPPER_MASK) != 0)?"ON":"OFF");
    }
  else
    {
    // turn unwrapper on/off
    
    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ON")==0)
        {
        writereg(1,readreg(1) | UNWRAPPER_MASK);
        snprintf(ans, maxlen, "%s: Unwrapper is now ON\n", OKS);
        }
      else if(strcmp(p,"OFF")==0)
        {
        writereg(1,readreg(1) & ~UNWRAPPER_MASK);
        snprintf(ans, maxlen, "%s: Unwrapper is now OFF\n", OKS);
        }
      else
        snprintf(ans, maxlen, "%s: use ON/OFF with UNWRAPper command\n", ERRS);
      }
    else
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseUNWRES(char *ans, size_t maxlen, int rw)
  {
  char *p;
  
  if(rw==READ)
    {
    // read unwrapper reset option on/off state
    snprintf(ans, maxlen, "%s: %s\n", OKS, ((readreg(1) & UNWRESET_MASK) != 0)?"ON":"OFF");
    }
  else
    {
    // turn unwrapper reset option on/off
    
    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ON")==0)
        {
        writereg(1,readreg(1) | UNWRESET_MASK);
        snprintf(ans, maxlen, "%s: Unwrapper reset option is now ON\n", OKS);
        }
      else if(strcmp(p,"OFF")==0)
        {
        writereg(1,readreg(1) & ~UNWRESET_MASK);
        snprintf(ans, maxlen, "%s: Unwrapper reset option is now OFF\n", OKS);
        }
      else
        snprintf(ans, maxlen, "%s: use ON/OFF with UNW_RES command\n", ERRS);
      }
    else
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseUNWTHR(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int n;
  
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
      errno=0;  // to distinguish success/failure after call
      df=strtof(p, NULL);
      if(errno!=0)
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
      errno=0;  // to distinguish success/failure after call
      g=strtof(p, NULL);
      if(errno!=0)
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

void parseMECOSCMD(char *ans, size_t maxlen, int rw)
  {
  int n;
  
  if(rw==READ)
    {
    // read current command to mecos; it's sfix_22.0
    n=(int)(readreg(5) & MECOSCMD_MASK);
    // sign extension
    n=(n ^ MECOSCMD_SIGN)-MECOSCMD_SIGN;
    snprintf(ans, maxlen, "%s: %+d pulses\n", OKS, n);
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);

  }


//-------------------------------------------------------------------

void parsePHERR(char *ans, size_t maxlen, int rw)
  {
  int n;
  float x;
  
  if(rw==READ)
    {
    // read current phase error; it's sfix_24.7
    n=(int)(readreg(6) & PHERR_MASK);
    // sign extension
    n=(n ^ PHERR_SIGN)-PHERR_SIGN;
    // convert the value to ns
    // 1 count = 8 ns
    // count value is fractional because filtered and decimated -> precision increases
    x=n/POW_2_7*8.;
    snprintf(ans, maxlen, "%s: %+f ns\n", OKS, x);
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);

  }


//-------------------------------------------------------------------

void parseLOCK(char *ans, size_t maxlen, int rw, unsigned int mask)
  {
  if(rw==READ)
    {
    // read lock status
    snprintf(ans, maxlen, "%s: %s\n", OKS, ((readreg(0) & mask) != 0)?"ON":"OFF");
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);

  }


//-------------------------------------------------------------------

void parseLOL(char *ans, size_t maxlen, int rw)
  {
  char *p;
  
  if(rw==READ)
    {
    // read sticky lock-of-loss alarm
    snprintf(ans, maxlen, "%s: %s\n", OKS, ((readreg(0) & STICKYLOL_MASK) != 0)?"ON":"OFF");
    }
  else
    {
    // reset sticky lock-of-loss alarm
    
    // next in line must be OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"OFF")==0)
        {
        writereg(1,readreg(1) | LOL_RESET_MASK);
        snprintf(ans, maxlen, "%s: Sticky loss-of-lock alarm has been reset\n", OKS);
        }
      else
        snprintf(ans, maxlen, "%s: use OFF to reset the sticky loss-of-lock alarm\n", ERRS);
      }
    else
      snprintf(ans, maxlen, "%s: missing 'OFF'\n", ERRS);
    }

  }


//-------------------------------------------------------------------

void parseMECOS_HZ_SETP(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int ret;
  long vsetpoint;
  
  if(rw==READ)
    {
    // read speed setpoint from MECOS AMB
    ret=can_hz_setpoint_read((unsigned long *)&vsetpoint);
    if(ret==0)
      snprintf(ans, maxlen, "%s: %ld Hz\n", OKS, vsetpoint);
    else
      snprintf(ans, maxlen, "%s: CAN error reading Hz Setpoint\n", ERRS);
    }
  else
    {
    // write speed setpoint to MECOS AMB

    // next in line is the desired speed setpoint
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      errno=0;  // to distinguish success/failure after call
      vsetpoint=strtol(p, NULL, 10);
      if(errno!=0)
        snprintf(ans, maxlen, "%s: invalid MECOS Hz setpoint value\n", ERRS);
      else
        {
        vsetpoint=(vsetpoint<=MECOS_MAX_SPEED)? vsetpoint : MECOS_MAX_SPEED;
        ret=can_hz_setpoint_write((unsigned long)vsetpoint);
      if(ret==0)
        snprintf(ans, maxlen, "%s: new MECOS Hz setpoint is %ld Hz\n", OKS, vsetpoint);
      else
        snprintf(ans, maxlen, "%s: CAN error writing Hz Setpoint\n", ERRS);
        }
      }
    else
      snprintf(ans, maxlen, "%s: missing MECOS Hz setpoint specification\n", ERRS);
    }
  }


//-------------------------------------------------------------------

void parseMECOS_HZ_ACT(char *ans, size_t maxlen, int rw)
  {
  int ret;
  long vact;
  
  if(rw==READ)
    {
    // read actual speed from MECOS AMB
    ret=can_hz_actual_read((unsigned long *)&vact);
    if(ret==0)
      snprintf(ans, maxlen, "%s: %ld Hz\n", OKS, vact);
    else
      snprintf(ans, maxlen, "%s: CAN error reading actual speed\n", ERRS);
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);
  }


//-------------------------------------------------------------------

void parseMECOS_LIFTUP(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int ret;
  bool lifted;
  long vact;
  
  if(rw==READ)
    {
    // read whether MECOS AMB is lifted or not 
    ret=can_liftup_state_read(&lifted);
    if(ret==0)
      snprintf(ans, maxlen, "%s: %s\n", OKS, lifted?"ON":"OFF");
    else
      snprintf(ans, maxlen, "%s: CAN error reading liftup state\n", ERRS);
    }
  else
    {
    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ON")==0)
        {
        ret=can_liftup_state_write(true);
        if(ret==0)
          snprintf(ans, maxlen, "%s: MECOS AMB lifted UP\n", OKS);
        else
          snprintf(ans, maxlen, "%s: CAN error writing liftup state\n", ERRS);
        }
      else if(strcmp(p,"OFF")==0)
        {
        // I won't lift down unless I can read that MECOS speed is zero
        vact=1L;
        ret=can_hz_actual_read((unsigned long *)&vact);
        if((ret!=0)||(vact|=0L))
          {
          snprintf(ans, maxlen, "%s: won't lift down when MECOS speed is not zero\n", ERRS);
          }
        else
          {
          ret=can_liftup_state_write(false);
          if(ret==0)
            snprintf(ans, maxlen, "%s: MECOS AMB lifted DOWN\n", OKS);
          else
            snprintf(ans, maxlen, "%s: CAN error writing liftup state\n", ERRS);
          }
        }
      else
        snprintf(ans, maxlen, "%s: use ON/OFF with MECOS:LIFTUP command\n", ERRS);
      }
    else
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", ERRS);
    }
  }


//-------------------------------------------------------------------

void parseMECOS_ROTATION(char *ans, size_t maxlen, int rw)
  {
  char *p;
  int ret;
  bool rotating;
  
  if(rw==READ)
    {
    // read whether MECOS AMB is rotating or not 
    ret=can_rotation_state_read(&rotating);
    if(ret==0)
      snprintf(ans, maxlen, "%s: %s\n", OKS, rotating?"ON":"OFF");
    else
      snprintf(ans, maxlen, "%s: CAN error reading rotating state\n", ERRS);
    }
  else
    {
    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ON")==0)
        {
        ret=can_rotation_state_write(true);
        if(ret==0)
          snprintf(ans, maxlen, "%s: MECOS AMB rotation ON\n", OKS);
        else
          snprintf(ans, maxlen, "%s: CAN error writing rotation state\n", ERRS);
        }
      else if(strcmp(p,"OFF")==0)
        {
        ret=can_rotation_state_write(false);
        if(ret==0)
          snprintf(ans, maxlen, "%s: MECOS AMB rotation OFF\n", OKS);
        else
          snprintf(ans, maxlen, "%s: CAN error writing rotation state\n", ERRS);
        }
      else
        snprintf(ans, maxlen, "%s: use ON/OFF with MECOS:ROTATION command\n", ERRS);
      }
    else
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", ERRS);
    }
  }


//-------------------------------------------------------------------

void parseMECOS_FAULT(char *ans, size_t maxlen, int rw)
  {
  int ret;
  unsigned long errcode;
  
  if(rw==READ)
    {
    // read general fault register from MECOS
    ret=can_general_fault_read(&errcode);
    if(ret==0)
      snprintf(ans, maxlen, "%s: %s\n", OKS, (errcode!=0UL)?"ON":"OFF");
    else
      snprintf(ans, maxlen, "%s: CAN error reading MECOS fault register\n", ERRS);
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);
  }


//-------------------------------------------------------------------

void parseMECOS_STABLE(char *ans, size_t maxlen, int rw)
  {
  int ret;
  bool enabled;
  
  if(rw==READ)
    {
    // ask MECOS whether external control is enabled
    ret=can_ext_ctl_enabled_read(&enabled);
    if(ret==0)
      snprintf(ans, maxlen, "%s: %s\n", OKS, enabled?"ON":"OFF");
    else
      snprintf(ans, maxlen, "%s: CAN error reading MECOS stable state (=ext control enable)\n", ERRS);
    }
  else
    snprintf(ans, maxlen, "%s: write operation not supported\n", ERRS);
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
  sendback(filedes,"UNWRAPper {ON|OFF}            : [advanced - be careful] turn unwrapper on or off\n");
  sendback(filedes,"UNWRAPper?                    : query unwrapper state; answer is either ON or OFF\n");
  sendback(filedes,"UNW_RES {ON|OFF}              : [advanced - be careful] turn unwrapper reset option on or off\n");
  sendback(filedes,"UNW_RES?                      : query unwrapper reset option; answer is either ON or OFF\n");
  sendback(filedes,"UNW_THR <value>               : [advanced - be careful] set threshold for unwrapper reset\n");
  sendback(filedes,"UNW_THR?                      : query the threshold for unwrapper reset\n");
  sendback(filedes,"SIGGEN_DF_HZ <value>          : [advanced - be careful] delta frequency for diagnostic bunch marker generator\n");
  sendback(filedes,"                                Frequency will be 3'123'437.5 + <value> Hz; <value> can be negative\n");
  sendback(filedes,"SIGGEN_DF_HZ?                 : query the diagnostic bunch marker generator delta frequency\n");
  sendback(filedes,"FLOCK?                        : query frequency lock; answer is either ON or OFF; read only\n");
  sendback(filedes,"PHLOCK?                       : query phase lock; answer is either ON or OFF; read only\n");
  sendback(filedes,"PHERR?                        : query current phase error in ns; read only\n");
  sendback(filedes,"MECOS_CMD?                    : query current inc/dec speed command from chopsync to MECOS; read only\n");
  sendback(filedes,"                                answer is number of commanded speed steps; a positive number means accelerate\n");
  sendback(filedes,"BUNCHFREQ?                    : query current bunch marker frequency in Hz; read only\n");
  sendback(filedes,"CHOPFREQ?                     : query current chopper photodiode frequency in Hz; read only\n");
  sendback(filedes,"STICKYLOL OFF                 : reset sticky loss-of-lock alarm; it can be set by hardware only\n");
  sendback(filedes,"STICKYLOL?                    : query sticky loss-of-lock alarm; answer is either ON or OFF\n");
  sendback(filedes,"Gain <value>                  : [advanced - be careful] set loop gain (conservative=4; high performance=default=6)\n");
  sendback(filedes,"Gain?                         : query loop gain\n");
  sendback(filedes,"MECOS:HZ_SETPoint <value>     : command <value> Hz as chopper rotation frequency to MECOS AMB; must be <= 1000 Hz\n");
  sendback(filedes,"MECOS:HZ_SETPoint?            : read current commanded rotation frequency of MECOS AMB (Hz)\n");
  sendback(filedes,"MECOS:HZ_ACTual?              : read actual rotation frequency of MECOS AMB (Hz)\n");
  sendback(filedes,"MECOS:LIFTUP {ON|OFF}         : lift up or down chopper active magnetic bearing (AMB);\n");
  sendback(filedes,"                                CAUTION! NEVER lift down if the chopper is ROTATING!\n");
  sendback(filedes,"MECOS:LIFTUP?                 : query chopper active magnetic bearing (AMB) lift state;\n");
  sendback(filedes,"                                returns ON (=lifted up) or OFF (=lifted down)\n");
  sendback(filedes,"MECOS:ROTation {ON|OFF}       : starts/stops chopper rotation\n");
  sendback(filedes,"MECOS:ROTation?               : query chopper rotation state\n");
  sendback(filedes,"MECOS:FAULT?                  : returns ON in case of any faults in MECOS AMB; OFF for no faults \n");
  sendback(filedes,"MECOS:STABLE?                 : returns ON if MECOS AMB rotation is stable and external control\n");
  sendback(filedes,"                                by the chopper synchronizer is possible; OFF means that AMB speed is not stable yet,\n");
  sendback(filedes,"                                so it is not possible to engage the chopper synchronizer\n");
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
    parseIDN(ans, maxlen);
  else if(strcmp(p,"*STB")==0)
    parseSTB(ans, maxlen);
  else if( (strcmp(p,"SYNCH")==0) || (strcmp(p,"SYNCHRONIZER")==0))
    parseSYNCHRONIZER(ans, maxlen, rw);
  else if(strcmp(p,"*RST")==0)
    parseRST(ans, maxlen);
  else if(strcmp(p,"PHSETPOINT_NS")==0)
    parsePHSETP(ans, maxlen, rw);
  else if(strcmp(p,"BUNCHMARKER_PRESCALER")==0)
    parsePRESCALER(ans, maxlen, rw, BUNCHMARKER_PSCALER_REG);
  else if(strcmp(p,"CHOPPER_PRESCALER")==0)
    parsePRESCALER(ans, maxlen, rw, CHOPPER_PSCALER_REG);
  else if(strcmp(p,"TRIGOUT_PH")==0)
    parseTRIGOUTPH(ans, maxlen, rw);
  else if( (strcmp(p,"UNWRAP")==0) || (strcmp(p,"UNWRAPPER")==0))
    parseUNWRAP(ans, maxlen, rw);
  else if(strcmp(p,"UNW_RES")==0)
    parseUNWRES(ans, maxlen, rw);
  else if(strcmp(p,"UNW_THR")==0)
    parseUNWTHR(ans, maxlen, rw);
  else if(strcmp(p,"SIGGEN_DF_HZ")==0)
    parseSIGGENDFTW(ans, maxlen, rw);
  else if( (strcmp(p,"G")==0) || (strcmp(p,"GAIN")==0))
    parseGAIN(ans, maxlen, rw);
  else if(strcmp(p,"FLOCK")==0)
    parseLOCK(ans, maxlen, rw, FREQUENCY);
  else if(strcmp(p,"PHLOCK")==0)
    parseLOCK(ans, maxlen, rw, PHASE);
  else if(strcmp(p,"MECOS_CMD")==0)
    parseMECOSCMD(ans, maxlen, rw);
  else if(strcmp(p,"PHERR")==0)
    parsePHERR(ans, maxlen, rw);
  else if(strcmp(p,"BUNCHFREQ")==0)
    parseFREQ(ans, maxlen, rw, BUNCHMARKER_FREQ_REG);
  else if(strcmp(p,"CHOPFREQ")==0)
    parseFREQ(ans, maxlen, rw, CHOPPER_FREQ_REG);
  else if(strcmp(p,"STICKYLOL")==0)
    parseLOL(ans, maxlen, rw);
  else if( (strcmp(p,"MECOS:HZ_SETP")==0) || (strcmp(p,"MECOS:HZ_SETPOINT")==0))
    parseMECOS_HZ_SETP(ans, maxlen, rw);
  else if( (strcmp(p,"MECOS:HZ_ACT")==0) || (strcmp(p,"MECOS:HZ_ACTUAL")==0))
    parseMECOS_HZ_ACT(ans, maxlen, rw);
  else if(strcmp(p,"MECOS:LIFTUP")==0)
    parseMECOS_LIFTUP(ans, maxlen, rw);
  else if( (strcmp(p,"MECOS:ROT")==0) || (strcmp(p,"MECOS:ROTATION")==0))
    parseMECOS_ROTATION(ans, maxlen, rw);
  else if(strcmp(p,"MECOS:FAULT")==0)
    parseMECOS_FAULT(ans, maxlen, rw);
  else if(strcmp(p,"MECOS:STABLE")==0)
    parseMECOS_STABLE(ans, maxlen, rw);
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

//int main(int argc, char *const argv[])
int main(void)
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

  // open CAN interface to talk to MECOS
  // register success into global "can_present"
  // if it fails, we proceed anyway, without CAN support
  can_present=open_can();
  if(can_present!=0)
    perror("CAN unavailable; continuing anyway");

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
                        (socklen_t * restrict) &size);
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
