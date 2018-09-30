/* tab:4
 *
 * input.c - source file for input control to maze game
 *
 * "Copyright (c) 2004-2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:        Steve Lumetta
 * Version:       7
 * Creation Date: Thu Sep 9 22:25:48 2004
 * Filename:      input.c
 * History:
 *    SL    1    Thu Sep 9 22:25:48 2004
 *        First written.
 *    SL    2    Sat Sep 12 14:34:19 2009
 *        Integrated original release back into main code base.
 *    SL    3    Sun Sep 13 03:51:23 2009
 *        Replaced parallel port with Tux controller code for demo.
 *    SL    4    Sun Sep 13 12:49:02 2009
 *        Changed init_input order slightly to avoid leaving keyboard
 *        in odd state on failure.
 *    SL    5    Sun Sep 13 16:30:32 2009
 *        Added a reasonably robust direct Tux control for demo mode.
 *    SL    6    Wed Sep 14 02:06:41 2011
 *        Updated input control and test driver for adventure game.
 *    SL    7    Wed Sep 14 17:07:38 2011
 *        Added keyboard input support when using Tux kernel mode.
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <termio.h>
#include <termios.h>
#include <unistd.h>

#include <errno.h>

#include "assert.h"
#include "input.h"

#include "module/tuxctl-ioctl.h"

/* set to 1 and compile this file by itself to test functionality */
#define TEST_INPUT_DRIVER 1

/* set to 1 to use tux controller; otherwise, uses keyboard input */
#define USE_TUX_CONTROLLER 1

#define TUX_BUTTON_RIGHT      0x80
#define TUX_BUTTON_LEFT       0x40
#define TUX_BUTTON_DOWN       0x20
#define TUX_BUTTON_UP         0x10
#define TUX_BUTTON_C          0x08
#define TUX_BUTTON_B          0x04
#define TUX_BUTTON_A          0x02
#define TUX_BUTTON_START      0x01

#define BYTE_OFFSET_1 0
#define BYTE_OFFSET_2 8
#define BYTE_OFFSET_3 16
#define BYTE_OFFSET_4 24


/* stores original terminal settings */
static struct termios tio_orig;

int fd;
void tux_init();
cmd_t get_tux_input();

/*
 * init_input
 *   DESCRIPTION: Initializes the input controller.  As both keyboard and
 *                Tux controller control modes use the keyboard for the quit
 *                command, this function puts stdin into character mode
 *                rather than the usual terminal mode.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 on success, -1 on failure
 *   SIDE EFFECTS: changes terminal settings on stdin; prints an error
 *                 message on failure
 */
int init_input() {
    struct termios tio_new;

    /*
     * Set non-blocking mode so that stdin can be read without blocking
     * when no new keystrokes are available.
     */
    if (fcntl(fileno(stdin), F_SETFL, O_NONBLOCK) != 0) {
        perror("fcntl to make stdin non-blocking");
        return -1;
    }

    /*
     * Save current terminal attributes for stdin.
     */
    if (tcgetattr(fileno(stdin), &tio_orig) != 0) {
        perror("tcgetattr to read stdin terminal settings");
        return -1;
    }

    /*
     * Turn off canonical(line-buffered) mode and echoing of keystrokes
     * to the monitor.  Set minimal character and timing parameters so as
     * to prevent delays in delivery of keystrokes to the program.
     */
    tio_new = tio_orig;
    tio_new.c_lflag &= ~(ICANON | ECHO);
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    if (tcsetattr(fileno(stdin), TCSANOW, &tio_new) != 0) {
        perror("tcsetattr to set stdin terminal settings");
        return -1;
    }

    /*Initialize the tux controller*/
    tux_init();

    /* Return success. */
    return 0;
}

static char typing[MAX_TYPED_LEN + 1] = {'\0'};

const char* get_typed_command() {
    return typing;
}

void reset_typed_command() {
    typing[0] = '\0';
}

static int32_t valid_typing(char c) {
    /* Valid typing include letters, numbers, space, and backspace/delete. */
    return (isalpha(c) || isdigit(c) || ' ' == c || 8 == c || 127 == c);
}

static void typed_a_char(char c) {
    int32_t len = strlen(typing);

    if (8 == c || 127 == c) {
        if (0 < len) {
            typing[len - 1] = '\0';
        }
    }
    else if (MAX_TYPED_LEN > len) {
        typing[len] = c;
        typing[len + 1] = '\0';
    }
}

/*
 * get_command
 *   DESCRIPTION: Reads a command from the input controller.  As some
 *                controllers provide only absolute input(e.g., go
 *                right), the current direction is needed as an input
 *                to this routine.
 *   INPUTS: cur_dir -- current direction of motion
 *   OUTPUTS: none
 *   RETURN VALUE: command issued by the input controller
 *   SIDE EFFECTS: drains any keyboard input
 */
cmd_t get_command() {

#if (USE_TUX_CONTROLLER == 0) /* use keyboard control with arrow keys */
    static int state = 0;                 /* small FSM for arrow keys */
#endif
    static cmd_t command = CMD_NONE;
    cmd_t pushed = CMD_NONE;
    int ch;

    /* Read all characters from stdin. */
    while ((ch = getc(stdin)) != EOF) {

        /* Backquote is used to quit the game. */
        if (ch == '`')
            return CMD_QUIT;

#if (USE_TUX_CONTROLLER == 0) /* use keyboard control with arrow keys */

        /*
         * Arrow keys deliver the byte sequence 27, 91, and 'A' to 'D';
         * we use a small finite state machine to identify them.
         *
         * Insert, home, and page up keys deliver 27, 91, '2'/'1'/'5' and
         * then a tilde.  We recognize the digits and don't check for the
         * tilde.
         */
        switch (state) {
            case 0:
                if (27 == ch) {
                    state = 1;
                }
                else if (valid_typing(ch)) {
                    typed_a_char(ch);
                }
                else if (10 == ch || 13 == ch) {
                    pushed = CMD_TYPED;
                }
                break;
            case 1:
                if (91 == ch) {
                    state = 2;
                }
                else {
                    state = 0;
                    if (valid_typing(ch)) {
                        /*
                         * Note that we may be discarding an ESC(27), but
                         * we don't use that as typed input anyway.
                         */
                        typed_a_char(ch);
                    }
                    else if (10 == ch || 13 == ch) {
                        pushed = CMD_TYPED;
                    }
                }
                break;
            case 2:
                if (ch >= 'A' && ch <= 'D') {
                    switch (ch) {
                        case 'A': pushed = CMD_UP;    break;
                        case 'B': pushed = CMD_DOWN;  break;
                        case 'C': pushed = CMD_RIGHT; break;
                        case 'D': pushed = CMD_LEFT;  break;
                    }
                    state = 0;
                }
                else if (ch == '1' || ch == '2' || ch == '5') {
                    switch (ch) {
                        case '2': pushed = CMD_MOVE_LEFT;  break;
                        case '1': pushed = CMD_ENTER;      break;
                        case '5': pushed = CMD_MOVE_RIGHT; break;
                    }
                    state = 3; /* Consume a '~'. */
                }
                else {
                    state = 0;
                    if (valid_typing(ch)) {
                        /*
                         * Note that we may be discarding an ESC(27) and
                         * a bracket(91), but we don't use either as
                         * typed input anyway.
                         */
                        typed_a_char(ch);
                    }
                    else if (10 == ch || 13 == ch) {
                        pushed = CMD_TYPED;
                    }
                }
                break;
            case 3:
                state = 0;
                if ('~' == ch) {
                    /* Consume it silently. */
                }
                else if (valid_typing(ch)) {
                    typed_a_char(ch);
                }
                else if (10 == ch || 13 == ch) {
                    pushed = CMD_TYPED;
                }
                break;
        }
#else /* USE_TUX_CONTROLLER */
        /* Tux controller mode; still need to support typed commands. */

        if (valid_typing(ch)) {
            typed_a_char(ch);
        }
        else if (10 == ch || 13 == ch) {
            pushed = CMD_TYPED;
        }
#endif /* USE_TUX_CONTROLLER */
    }

    if(pushed != CMD_TYPED){
          pushed = get_tux_input();
   }
    /*
     * Once a direction is pushed, that command remains active
     * until a turn is taken.
     */
    if (pushed == CMD_NONE) {
        command = CMD_NONE;
    }

    if(command != CMD_NONE){
          pushed = command;
   }

    return pushed;
}

/*
 * shutdown_input
 *   DESCRIPTION: Cleans up state associated with input control.  Restores
 *                original terminal settings.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: restores original terminal settings
 */
void shutdown_input() {
      close(fd);
      (void)tcsetattr(fileno(stdin), TCSANOW, &tio_orig);
}

void tux_init(){
      if((fd=open("/dev/ttyS0", O_RDWR | O_NOCTTY))){
            printf("/dev/ttyS0 open message: %s\n", strerror(errno));
      }

      int ldisc_num = N_MOUSE;

      if(ioctl(fd, TIOCSETD, &ldisc_num)){
            printf("IOCTL initialization failure: %s\n", strerror(errno));
      }

      printf("Calling first ioctl\n");
      if(ioctl(fd, TUX_INIT, NULL)){
            printf("TUX_INIT IOCTL failure: %s\n", strerror(errno));
      }
      return;
}

short hex_to_BCD(short number){
      short tens_place;
      short ones_place;

      tens_place = number / 10;
      ones_place = number % 10;

      return (tens_place << 4) | (ones_place << BYTE_OFFSET_1);
}

/*
 * display_time_on_tux
 *   DESCRIPTION: Show number of elapsed seconds as minutes:seconds
 *                on the Tux controller's 7-segment displays.
 *   INPUTS: num_seconds -- total seconds elapsed so far
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes state of controller's display
 */
void display_time_on_tux(int num_seconds) {
      unsigned long tux_data = 0;
      short decimal_points_on;
      short displays_on;
      int minutes;
      int seconds;

      minutes = hex_to_BCD(num_seconds / 60);
      seconds = hex_to_BCD(num_seconds % 60);
      decimal_points_on = 0x2;
      displays_on = 0x7;

      if(minutes > 0x9){
            displays_on = 0x0F;
      }

      tux_data |= (seconds << BYTE_OFFSET_1);
      tux_data |= (minutes << BYTE_OFFSET_2);
      tux_data |= (displays_on << BYTE_OFFSET_3);
      tux_data |= (decimal_points_on << BYTE_OFFSET_4);

      if(ioctl(fd, TUX_SET_LED, tux_data)){
            printf("display_time_on_tux error: %d\n", errno);
      }

      ioctl(fd, TUX_SET_LED, tux_data);

      return;

}

cmd_t get_tux_input(){
      long  button_pressed;

      ioctl(fd, TUX_BUTTONS, &button_pressed);

      switch(button_pressed){
            case TUX_BUTTON_UP:     return CMD_UP;
            case TUX_BUTTON_DOWN:   return CMD_DOWN;
            case TUX_BUTTON_LEFT:   return CMD_LEFT;
            case TUX_BUTTON_RIGHT:  return CMD_RIGHT;
            case TUX_BUTTON_A:      return CMD_MOVE_LEFT;
            case TUX_BUTTON_B:      return CMD_ENTER;
            case TUX_BUTTON_C:      return CMD_MOVE_RIGHT;
            default:                return CMD_NONE;
      }

      return CMD_NONE;
}

#if (TEST_INPUT_DRIVER == 1)
int main() {
    cmd_t last_cmd = CMD_NONE;
    cmd_t cmd;
    static const char* const cmd_name[NUM_COMMANDS] = {
        "none", "right", "left", "up", "down", "move left",
        "enter", "move right", "typed command", "quit"
    };

    /* Grant ourselves permission to use ports 0-1023 */
    if (ioperm(0, 1024, 1) == -1) {
        perror("ioperm");
        return 3;
    }

    init_input();
    while (1) {
       while ((cmd = get_command()) == last_cmd);
       last_cmd = cmd;
       printf("command issued: %s\n", cmd_name[cmd]);
       display_time_on_tux(83);
       if (cmd == CMD_QUIT)
            break;
      }
      shutdown_input();
      return 0;
}

#endif
