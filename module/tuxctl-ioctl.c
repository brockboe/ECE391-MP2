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

long button_status;
long led_status;
long led_debug = 0x0F0FABCD;

void turn_bioc_on(struct tty_struct *tty);
void turn_leds_on(struct tty_struct *tty);
void turn_dbg_off(struct tty_struct * tty);
void handle_bioc_event(short b, short c);
void handle_reset_request(struct tty_struct *tty);
void set_leds(unsigned long arg, struct tty_struct *tty);

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
      0xEE,       /*A*/
      0x6D,       /*b*/
      0x49,       /*c*/
      0x4F,       /*d*/
      0xE9,       /*E*/
      0xE7        /*F*/
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

    printk("packet : %x %x %x\n", a, b, c);


      switch(a) {
            case MTCP_BIOC_EVENT:
                  printk("BIOC event has occured\n");
                  handle_bioc_event((short)b, (short)c);
                  return;
            case MTCP_RESET:
                  printk("We have received a reset request\n");
                  handle_reset_request(tty);
                  return;
            case MTCP_ACK:
                  printk("We received an ACK\n");
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
            turn_dbg_off(tty);
            button_status = 0;
            led_status = 0;
            turn_bioc_on(tty);
            turn_leds_on(tty);
            return 0;

        case TUX_BUTTONS:
            if((unsigned long *)arg == NULL){
                  return -EINVAL;
            }
            *(unsigned long *)arg = button_status;
            return 0;

        case TUX_SET_LED:
            set_leds(arg, tty);
            return 0;

        default:
            return -EINVAL;
    }
}

void handle_bioc_event(short b, short c){
      int buttons_low = b & DATA_MASK;
      int buttons_high = (c & DATA_MASK) << 4;
      button_status = (long)(buttons_high | buttons_low);

      return;
}

void set_leds(unsigned long arg, struct tty_struct *tty){
      short display_write_mask = 0x0F;
      short led_on = (arg & 0x000F0000) >> 16;
      short dot_on = (arg & 0x0F000000) >> 24;
      short outbuf[6] = {MTCP_LED_SET, led_on, 0, 0, 0, 0};
      int data = arg & 0x0000FFFF;
      int i;

      int bytes_to_send = 2;

      led_status = arg;

      for(i = 0; i < 4; i ++){
            if((led_on & (1 << i)) != 0){
                  outbuf[bytes_to_send] = hex_to_display[(((data & (0xF << i*4))) >> i*4)];
                  outbuf[bytes_to_send] |= ((dot_on & (1 << i)) << (4-i));
                  bytes_to_send++;
            }
      }

      if(tuxctl_ldisc_put((struct tty_struct *)tty, (char const *)outbuf, bytes_to_send)){
            printk("set_leds: Not all data was sent to device\n");
      }

      return;
}

void handle_reset_request(struct tty_struct *tty){
      turn_dbg_off(tty);
      turn_leds_on(tty);
      set_leds(led_debug, tty);
      turn_bioc_on(tty);
      return;
}

void turn_bioc_on(struct tty_struct *tty){
      char const outbuf = MTCP_BIOC_ON;
      printk("turning bioc on\n");
      if(tuxctl_ldisc_put(tty, &outbuf, 1)){
            printk("turn_bioc_on failure: Not all data was sent to device\n");
      }
      return;
}

void turn_leds_on(struct tty_struct *tty){
      char const outbuf = MTCP_LED_USR;
      printk("turning leds on\n");
      if(tuxctl_ldisc_put(tty, &outbuf, 1)){
            printk("turn_leds_on failure: Not all data was sent to device\n");
      }
      return;
}

void turn_dbg_off(struct tty_struct *tty){
      char const outbuf = MTCP_DBG_OFF;
      printk("Turning dbg off\n");
      if(tuxctl_ldisc_put(tty, &outbuf, 1)){
            printk("turn_dbg_off failrure: Not all data was sent to device\n");
      }
      return;
}
