/*
 * tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

/************************ Protocol Implementation *************************/

#define BYTE_LOWER 0x0F
#define BYTE_UPPER 0xF0
#define LED_DISPLAY_OFFSET 16
#define LED_DOT_OFFSET 24
#define BIT_5_MASK 0x20
#define BIT_6_MASK 0x40

unsigned long button_status;

void reset_handler(struct tty_struct * tty);
void init_handler(struct tty_struct * tty);
void led_handler(struct tty_struct * tty, unsigned long arg);
void bioc_event_handler(struct tty_struct * tty, char a, char b);

short hex_to_display[16] = {
      0xE7,       /*0*/
      0x06,       /*1*/
      0xCB,       /*2*/
      0x8F,       /*3*/
      0x2E,       /*4*/
      0xAD,       /*5*/
      0xED,       /*6*/
      0x86,       /*7*/
      0xEF,       /*8*/
      0xAF,       /*9*/
      0xEE,       /*A - uppercase*/
      0x6D,       /*b - lowercase*/
      0x49,       /*c - lowercase*/
      0x4F,       /*d - lowercase*/
      0xE9,       /*E - uppercase*/
      0xE7,       /*F - uppercase*/
};

#define DATA_MASK 0x0F

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in
 * tuxctl-ld.c. It calls this function, so all warnings there apply
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet) {
    unsigned a, b, c;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

      switch(a) {
            case MTCP_BIOC_EVENT:
                  bioc_event_handler(tty, b, c);
                  return;
            case MTCP_RESET:
                  reset_handler(tty);
                  return;
            case MTCP_ACK:
                  return;
            default:
                  return;
      }
      return;
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/

int tuxctl_ioctl(struct tty_struct* tty, struct file* file,
                 unsigned cmd, unsigned long arg) {
    switch (cmd) {
        case TUX_INIT:
            init_handler(tty);
            return 0;

        case TUX_BUTTONS:
            if(arg == 0){
                  return -EINVAL;
            }
            *(unsigned long *)arg = button_status;
            return 0;

        case TUX_SET_LED:
            led_handler(tty, arg);
            return 0;

        default:
            return -EINVAL;
    }
}

void bioc_event_handler(struct tty_struct * tty, char b, char c){
      char down_press;
      char left_press;
      button_status = 0xFFFFFF00;
      button_status |= (long)(b & BYTE_LOWER);
      button_status |= (long)((c & BYTE_LOWER) << 4);

      /*button_status value at this point is R D L U C B A S*/
      /*and we want R L D U C B A S, so we need to flip bits 5 and 6*/
      button_status = ~button_status;

      /*Grab the status of the left and down buttons*/
      down_press = button_status & BIT_6_MASK;
      left_press = button_status & BIT_5_MASK;

      /*Now swap the values*/
      button_status &= ~BIT_5_MASK;
      button_status &= ~BIT_6_MASK;
      if(down_press){
            button_status |= BIT_5_MASK;
      }
      if(left_press){
            button_status |= BIT_6_MASK;
      }

      return;
}

void reset_handler(struct tty_struct * tty){
      const char outbuf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
      tuxctl_ldisc_put(tty, outbuf, 2);
      button_status = 0;
      return;
}

void init_handler(struct tty_struct * tty){
      const char outbuf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
      tuxctl_ldisc_put(tty, outbuf, 2);
      button_status = 0;
      return;
}

void led_handler(struct tty_struct * tty, unsigned long arg){
      short data_to_print;
      char decimal_points_on;
      char led_displays_on;
      char led[4];
      int i;
      char outbuf[6] = {MTCP_LED_SET, BYTE_LOWER, 0, 0, 0, 0};

      data_to_print     = arg & LED_DISPLAY_MASK;
      led_displays_on   = (arg & DISPLAY_ON_MASK) >> DISPLAY_ON_OFFSET;
      decimal_points_on = (arg & DECIMAL_POINT_MASK) >> DECIMAL_POINT_OFFSET;

      led[0] = hex_to_display[(data_to_print & LED_DISPLAY0_MASK) >> LED_DISPLAY0_OFFSET];
      led[1] = hex_to_display[(data_to_print & LED_DISPLAY1_MASK) >> LED_DISPLAY1_OFFSET];
      led[2] = hex_to_display[(data_to_print & LED_DISPLAY2_MASK) >> LED_DISPLAY2_OFFSET];
      led[3] = hex_to_display[(data_to_print & LED_DISPLAY3_MASK) >> LED_DISPLAY3_OFFSET];

      for(i = 0; i < 4; i++){
            if(led_displays_on & (1<<i)){
                  outbuf[i+2] = led[i];
            }
            if(decimal_points_on & (1<<i)){
                  outbuf[i+2] |= DECIMAL_POINT_BIT;
            }
      }

      tuxctl_ldisc_put(tty, (const char *)outbuf, 6);

      return;
}
