/**************************************************
 ***                                            ***
 ***  chopsync CAN interface to MECOS           ***
 ***                                            ***
 ***  latest rev: aug  6 2024                   ***
 ***                                            ***
 **************************************************/ 
// code derived from inno-maker USB-CAN interface sample code:
// https://github.com/INNO-MAKER/usb2can/blob/master/For%20Linux%20Raspbian%20Ubuntu/software/c/can_send.c
//
// See also kernel documentation of its standard SocketCAN driver:
// https://www.kernel.org/doc/Documentation/networking/can.txt

#ifndef CANMECOS_H
#define CANMECOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <errno.h>
#include <sys/time.h>
// Special address description flags for CAN_ID
#define CAN_EFF_FLAG 0x80000000U
#define CAN_RTR_FLAG 0x40000000U
#define CAN_ERR_FLAG 0x20000000U

#define RX_TIMEOUT_SEC 4


/******* protos *******/

int open_can(void);
int close_can(void);
int can_read_register(unsigned char addr_hi, unsigned char addr_lo, unsigned char subindex, unsigned long int *val);
int can_write_register(unsigned char addr_hi, unsigned char addr_lo, unsigned char subindex, unsigned long int val);
int can_wait_answer(struct can_frame *match_frame, unsigned long int *response);
int can_hz_setpoint_write(unsigned long int setpoint_hz);
int can_hz_setpoint_read(unsigned long int *setpoint_hz_ptr);
int can_hz_actual_read(unsigned long int *speed_hz_ptr);

#endif
