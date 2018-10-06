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

/*Global variable in which the status of the buttons is stored*/
unsigned long button_status;

/*See the function descriptions beleow for more details*/
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
                  /*Call the bioc event helper function*/
                  bioc_event_handler(tty, b, c);
                  return;
            case MTCP_RESET:
                  /*Call the reset helper function*/
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
            /*Call the init helper function*/
            init_handler(tty);
            return 0;

        case TUX_BUTTONS:
            /*Check for a null pointer*/
            if(arg == 0){
                  return -EINVAL;
            }
            /*write the value of the buttons to the argument*/
            *(unsigned long *)arg = button_status;
            return 0;

        case TUX_SET_LED:
            /*Call the led handler function*/
            led_handler(tty, arg);
            return 0;

        default:
            return -EINVAL;
    }
}

/*
 * void bioc_event_handler(struct tty_struct * tty, char b, char c)
 * DESCRIPTION:   This is called whenever a BIOC event occurs, reads
 *                the values of the buttons and stores them in the 
 *                button_status global variable.
 * INPUTS:        tty - the location from where the line discipline
 *                gets the button input
 *                b - second byte given to the program by the line discipline
 *                c - third byte given to the program by the line discipline
 * OUTPUTS:       none
 * SIDE EFFECTS:  Takes the second and third bytes given from the line
 *                discipline from a BIOC event and writes the status of the
 *                buttons to the button_status global variable
 */

void bioc_event_handler(struct tty_struct * tty, char b, char c){
      char down_press;  /*Holds the value of the down button being pressed*/
      char left_press;  /*Holds the vlaue of the left button being pressed*/
      /*Write the status of the buttons to the button_status variable*/
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

/*
 * reset_handler(struct tty_struct * tty)
 * DESCRIPTION:   handles resets from the tux controller, it resets the
 *                status of the controller to the way it was originally
 * INPUTS:        tty - The line discipline location to where to write
 *                the output commands
 * OUTPUTS:       none
 * SIDE EFFECTS:  resets the tux controller to the original status
 */

void reset_handler(struct tty_struct * tty){
      const char outbuf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
      /*Write the commands to the tux controller*/
      tuxctl_ldisc_put(tty, outbuf, 2);
      /*Reset the button status*/
      button_status = 0;
      return;
}

/*
 * init_handler(struct tty_struct * tty)
 * DESCRIPTION:   Handles the initialization of the tux controller. Does
 *                essentially the same thing as the reset handler helper
 *                function.
 * INPUTS:        tty - the location of the line discipline to write commands
 *                to and from
 * OUTPUTS:       none
 * SIDE EFFECTS:  initializes the tux controller
 */

void init_handler(struct tty_struct * tty){
      const char outbuf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
      tuxctl_ldisc_put(tty, outbuf, 2);
      button_status = 0;
      return;
}

/*
 * led_handler(struct tty_struct * tty, unsigned long arg)
 * DESCRIPTION:   Takes an argment arg containing the data
 *                to print and writes it to the led display on the
 *                tux controller
 * INPUTS:        tty - line discipline to write commands to
 *                arg - argument containing information to print
 * OUTPUTS:       none
 * SIDE EFFECTS:  changes value written on tux controller
 */

void led_handler(struct tty_struct * tty, unsigned long arg){
      short data_to_print;    /*Holds the four hexidecimal values to print*/
      char decimal_points_on; /*Bit mask of which decimal poitns to turn on*/
      char led_displays_on;   /*Bit mask of which leds to turn on */
      char led[4];            /*Array of values that describe which parts of the display to turn on*/
      int i;

      /*outbuf is used to hold the commands to write to the tux controller*/
      char outbuf[6] = {MTCP_LED_SET, BYTE_LOWER, 0, 0, 0, 0};

      /*From the argument, get the display mask, decimal mask, and the data to print*/
      data_to_print     = arg & LED_DISPLAY_MASK;
      led_displays_on   = (arg & DISPLAY_ON_MASK) >> DISPLAY_ON_OFFSET;
      decimal_points_on = (arg & DECIMAL_POINT_MASK) >> DECIMAL_POINT_OFFSET;

      /*Write the 7 segment display data into the arrays*/
      led[0] = hex_to_display[(data_to_print & LED_DISPLAY0_MASK) >> LED_DISPLAY0_OFFSET];
      led[1] = hex_to_display[(data_to_print & LED_DISPLAY1_MASK) >> LED_DISPLAY1_OFFSET];
      led[2] = hex_to_display[(data_to_print & LED_DISPLAY2_MASK) >> LED_DISPLAY2_OFFSET];
      led[3] = hex_to_display[(data_to_print & LED_DISPLAY3_MASK) >> LED_DISPLAY3_OFFSET];

      /*Write the values to the outbuf*/
      for(i = 0; i < 4; i++){
            /*Ensure the the display is turned on*/
            if(led_displays_on & (1<<i)){
                  outbuf[i+2] = led[i];
            }
            /*Turn the decimal point on if necessary*/
            if(decimal_points_on & (1<<i)){
                  outbuf[i+2] |= DECIMAL_POINT_BIT;
            }
      }

      /*Write the data to the console*/
      tuxctl_ldisc_put(tty, (const char *)outbuf, 6);

      return;
}
