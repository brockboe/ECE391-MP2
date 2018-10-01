#ifndef TUXCTL_H
#define TUXCTL_H

#define TUX_SET_LED _IOR('E', 0x10, unsigned long)
#define TUX_READ_LED _IOW('E', 0x11, unsigned long*)
#define TUX_BUTTONS _IOW('E', 0x12, unsigned long*)
#define TUX_INIT _IO('E', 0x13)
#define TUX_LED_REQUEST _IO('E', 0x14)
#define TUX_LED_ACK _IO('E', 0x15)

#endif

/*Values used for writing values to the LED display*/
#define LED_DISPLAY_MASK 0x0000FFFF
#define LED_DISPLAY0_MASK 0x000F
#define LED_DISPLAY1_MASK 0x00F0
#define LED_DISPLAY2_MASK 0x0F00
#define LED_DISPLAY3_MASK 0xF000
#define LED_DISPLAY0_OFFSET 0
#define LED_DISPLAY1_OFFSET 4
#define LED_DISPLAY2_OFFSET 8
#define LED_DISPLAY3_OFFSET 12
#define DECIMAL_POINT_MASK 0x0F000000
#define DECIMAL_POINT_OFFSET 24
#define DISPLAY_ON_MASK 0x000F0000
#define DISPLAY_ON_OFFSET 16
#define DECIMAL_POINT_BIT 0x10
