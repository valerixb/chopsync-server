/**************************************************
 ***                                            ***
 ***  chopsync CAN interface to MECOS           ***
 ***                                            ***
 ***  latest rev: aug  8 2024                   ***
 ***                                            ***
 **************************************************/ 
// code derived from inno-maker USB-CAN interface sample code:
// https://github.com/INNO-MAKER/usb2can/blob/master/For%20Linux%20Raspbian%20Ubuntu/software/c/can_send.c
//
// See also kernel documentation of its standard SocketCAN driver:
// https://www.kernel.org/doc/Documentation/networking/can.txt

#include "can.h"

/***  globals  ***/
int can_sock;

int open_can(void)
  {
  int ret;
  struct sockaddr_can addr;
  struct ifreq ifr;
  struct can_filter rfilter[1];
  struct timeval tv;

  // must close can device before set baud rate!
  system("sudo ifconfig can0 down");
  //below mean depend on iprout tools ,not ip tool with busybox
  system("sudo ip link set can0 type can bitrate 1000000");
  //system("sudo echo 1000000 > /sys/class/net/can0/can_bittiming/bitrate");
  system("sudo ifconfig can0 up");
  
  // create socket
  can_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if(can_sock < 0)
    {
    perror("Create socket PF_CAN failed!");
    return -1;
    }

  // specify can0 device
  strcpy(ifr.ifr_name, "can0");
  ret = ioctl(can_sock, SIOCGIFINDEX, &ifr);
  if(ret < 0)
    {
    perror("ioctl interface index failed!");
    return -1;
    }

  // set receive timeout (mus be done before bind)
  tv.tv_sec = RX_TIMEOUT_SEC;
  tv.tv_usec = 0;
  setsockopt(can_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

  // bind the socket to can0
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;
  ret = bind(can_sock, (struct sockaddr *)&addr, sizeof(addr));
  if(ret < 0)
    {
    perror("bind failed!");
    return -1;
    }

  // setup receive filter rules
  // receive only Ans_MPDO messages from MECOS AMB
  rfilter[0].can_id = 0x2C0;
  rfilter[0].can_mask = CAN_SFF_MASK;
  setsockopt(can_sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
  //setsockopt(can_sock, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

  return 0;
  }


//-------------------------------------------------------------------

int close_can(void)
  {
  close(can_sock);
  system("sudo ifconfig can0 down");
  return 0;  
  }


//-------------------------------------------------------------------

int can_write_register(unsigned char addr_hi, unsigned char addr_lo, unsigned char subindex, unsigned long int val)
  {
  struct can_frame frame;
  int nbytes;

  memset(&frame, 0, sizeof(struct can_frame));

  // assembly message data
  frame.can_id = 0x1C0;
  // payload length in byte (0..8)
  frame.can_dlc = 8;
  frame.data[0] = 0xC0;
  frame.data[1] = addr_lo;
  frame.data[2] = addr_hi;
  frame.data[3] = subindex;
  frame.data[4] = (unsigned char)(val & 0x000000FF);
  frame.data[5] = (unsigned char)((val>>8) & 0x000000FF);
  frame.data[6] = (unsigned char)((val>>16) & 0x000000FF);
  frame.data[7] = (unsigned char)((val>>24) & 0x000000FF);

  // send message out
  nbytes = write(can_sock, &frame, sizeof(frame)); 
  if(nbytes != sizeof(frame))
    {
    perror("CAN frame only partially sent\n");
    return -1;
    }

  return 0;
  }


//-------------------------------------------------------------------

int can_read_register(unsigned char addr_hi, unsigned char addr_lo, unsigned char subindex, unsigned long int *val)
  {
  struct can_frame frame;
  int nbytes;

  memset(&frame, 0, sizeof(struct can_frame));
  
  // send request to MECOS using a REQ_MPDO message

  // assembly message data
  frame.can_id = 0x340;
  // payload length in byte (0..8)
  frame.can_dlc = 4;
  frame.data[0] = 0xC0;
  frame.data[1] = addr_lo;
  frame.data[2] = addr_hi;
  frame.data[3] = subindex;

  // send message out
  nbytes = write(can_sock, &frame, sizeof(frame)); 
  if(nbytes != sizeof(frame))
    {
    perror("CAN frame only partially sent\n");
    return -1;
    }

  // now wait for answer
  // fill in the values we expect as answer, to filter out
  // other messages on can bus we are not interested in
  frame.can_id  = 0x2C0;
  frame.data[0] = 0x40;
  frame.data[1] = addr_lo;
  frame.data[2] = addr_hi;
  frame.data[3] = subindex;

  return(can_wait_answer(&frame, val));
  }


//-------------------------------------------------------------------

int can_wait_answer(struct can_frame *match_frame, unsigned long int *response)
  {
  struct can_frame frame;
  int nbytes;
  struct timeval tv1, tv2;
  double dt;
  
  // now wait for MECOS response via an Ans_MPDO message
  // note that also other CAN nodes may be on the bus, making different
  // requests to MECOS AMB, so there may be stray Ans_MPDOs on the bus
  // keep looking for the right one until we time out
  dt=0;
  gettimeofday(&tv1, NULL);
  while(dt<RX_TIMEOUT_SEC)
    {
    errno=0;
    // note that read has a timeout previously set via setsockopt
    nbytes = read(can_sock, &frame, sizeof(frame));
    if((errno==EAGAIN)||(errno==EWOULDBLOCK))
      {
      perror("Timed out");
      return -1;
      }
    if(nbytes > 0)
      {
      if(match_frame!=NULL)
        {
        if( ((frame.can_id&0x1FFFFFFF) == match_frame->can_id) &&
            (frame.can_dlc == 8) &&
            (frame.data[0] == match_frame->data[0]) &&
            (frame.data[1] == match_frame->data[1]) &&
            (frame.data[2] == match_frame->data[2]) &&
            (frame.data[3] == match_frame->data[3])
          )
          {
          // we received the correct message; now decode speed value
          if(response!=NULL)
            {
            *response=((unsigned long int)(frame.data[4]))+
                      (((unsigned long int)(frame.data[5]))<<8)+
                      (((unsigned long int)(frame.data[6]))<<16)+
                      (((unsigned long int)(frame.data[7]))<<24)
                      ;
            break;
            }
          else
            {
            perror("Null ptr passed for CAN readout");
            return -1;
            }
          }
        }
      else
        {
        perror("Null ptr passed for CAN frame match");
        return -1;
        }
      }
    
    gettimeofday(&tv2, NULL);
    dt= (tv2.tv_sec+tv2.tv_usec/1.e6) - (tv1.tv_sec+tv1.tv_usec/1.e6);
    }

  if(dt>=RX_TIMEOUT_SEC)
    {
    perror("Timed out");
    return -1;
    }

  return 0;
  }


//-------------------------------------------------------------------

int can_hz_setpoint_write(unsigned long int setpoint_hz)
  {
  return(can_write_register(0x20, 0x00, 0x00, setpoint_hz));
  }


//-------------------------------------------------------------------

int can_hz_setpoint_read(unsigned long int *setpoint_hz_ptr)
  {
  return(can_read_register(0x20, 0x00, 0x00, setpoint_hz_ptr));
  }


//-------------------------------------------------------------------

int can_hz_actual_read(unsigned long int *speed_hz_ptr)
  {
  return(can_read_register(0x20, 0x01, 0x00, speed_hz_ptr));
  }


//-------------------------------------------------------------------

int can_liftup_state_read(bool *lifted)
  {
  unsigned long val;
  int ret;

  ret=can_read_register(0x20, 0x0C, 0x00, &val);
  if(ret==0)
    *lifted = (val!=0);
  
  return ret;
  }


//-------------------------------------------------------------------

int can_liftup_state_write(bool lifted)
  {
  // different CAN registers are used to lift up or down
  if(lifted)
    return(can_write_register(0x20, 0x11, 0x00, 1UL));
  else
    return(can_write_register(0x20, 0x12, 0x00, 1UL));
  }


//-------------------------------------------------------------------

int can_general_fault_read(unsigned long int *fault_ptr)
  {
  return(can_read_register(0x20, 0x87, 0x00, fault_ptr));
  }


//-------------------------------------------------------------------

int can_rotation_state_read(bool *rotating)
  {
  unsigned long val;
  int ret;

  ret=can_read_register(0x20, 0x80, 0x00, &val);
  if(ret==0)
    *rotating = (val!=0);
  
  return ret;
  }


//-------------------------------------------------------------------

int can_rotation_state_write(bool rotating)
  {
  // different CAN registers are used to start or stop rotation
  if(rotating)
    return(can_write_register(0x20, 0x0F, 0x00, 1UL));
  else
    return(can_write_register(0x20, 0x10, 0x00, 1UL));
  }


//-------------------------------------------------------------------

int can_ext_ctl_enabled_read(bool *enabled)
  {
  unsigned long val;
  int ret;
  // CHANGE ME!!!!! we need the right register address from MECOS
  ret=can_read_register(0x20, 0x25, 0x00, &val);
  if(ret==0)
    *enabled = (val!=0);
  
  return ret;
  }
