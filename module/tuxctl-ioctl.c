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

long button_status;

void reset_handler(struct tty_struct * tty);
void init_handler(struct tty_struct * tty);

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
      0xE7        /*F - uppercase*/
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
                  button_status = (long)(b & BYTE_LOWER);
                  button_status |= (long)((c & BYTE_LOWER) << 4);
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
            if(arg == NULL){
                  return -EINVAL;
            }
            *arg = (unsigned long)button_status;
            return 0;

        case TUX_SET_LED:
            return 0;

        default:
            return -EINVAL;
    }
}

void reset_handler(struct tty_struct * tty){
      outbuf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
      tuxctl_ldisc_put(tty, outbuf, 2);
      button_Status = 0;
      return;
}

void init_handler(struct tty_struct * tty){
      outbuf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
      tuxctl_ldisc_put(tty, outbuf, 2);
      button_status = 0;
      return;
}
