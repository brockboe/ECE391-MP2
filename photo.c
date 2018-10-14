/* tab:4
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
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
 * Version:       3
 * Creation Date: Fri Sep 9 21:44:10 2011
 * Filename:      photo.c
 * History:
 *    SL    1    Fri Sep 9 21:44:10 2011
 *        First written(based on mazegame code).
 *    SL    2    Sun Sep 11 14:57:59 2011
 *        Completed initial implementation of functions.
 *    SL    3    Wed Sep 14 21:49:44 2011
 *        Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"
#include "types.h"


/* types local to this file(declared in types.h) */

/*
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;            /* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/*
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have
 * been stored as 2:2:2-bit RGB values(one byte each), including
 * transparent pixels(value OBJ_CLR_TRANSP).  As with the room photos,
 * pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of the
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;  /* defines height and width */
    uint8_t*       img;  /* pixel data               */
};


/* file-scope variables */

/*
 * The room currently shown on the screen.  This value is not known to
 * the mode X code, but is needed when filling buffers in callbacks from
 * that code(fill_horiz_buffer/fill_vert_buffer).  The value is set
 * by calling prep_room.
 */
static const room_t* cur_room = NULL;

void gen_color_pallette(uint16_t * raw_color_data, photo_t * p);

/*
 * fill_horiz_buffer
 *   DESCRIPTION: Given the(x,y) map pixel coordinate of the leftmost
 *                pixel of a line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS:(x,y) -- leftmost pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_horiz_buffer(int x, int y, unsigned char buf[SCROLL_X_DIM]) {
    int            idx;   /* loop index over pixels in the line          */
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */
    int            yoff;  /* y offset into object image                  */
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo(cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ? view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate(cur_room); NULL != obj; obj = obj_next(obj)) {
        obj_x = obj_get_x(obj);
        obj_y = obj_get_y(obj);
        img = obj_image(obj);

        /* Is object outside of the line we're drawing? */
        if (y < obj_y || y >= obj_y + img->hdr.height || x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
            continue;
        }

        /* The y offset of drawing is fixed. */
        yoff = (y - obj_y) * img->hdr.width;

        /*
         * The x offsets depend on whether the object starts to the left
         * or to the right of the starting point for the line being drawn.
         */
        if (x <= obj_x) {
            idx = obj_x - x;
            imgx = 0;
        }
        else {
            idx = 0;
            imgx = x - obj_x;
        }

        /* Copy the object's pixel data. */
        for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
            pixel = img->img[yoff + imgx];

            /* Don't copy transparent pixels. */
            if (OBJ_CLR_TRANSP != pixel) {
                buf[idx] = pixel;
            }
        }
    }
}


/*
 * fill_vert_buffer
 *   DESCRIPTION: Given the(x,y) map pixel coordinate of the top pixel of
 *                a vertical line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS:(x,y) -- top pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_vert_buffer(int x, int y, unsigned char buf[SCROLL_Y_DIM]) {
    int            idx;   /* loop index over pixels in the line          */
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */
    int            xoff;  /* x offset into object image                  */
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo(cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ? view->img[view->hdr.width *(y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate(cur_room); NULL != obj; obj = obj_next(obj)) {
        obj_x = obj_get_x(obj);
        obj_y = obj_get_y(obj);
        img = obj_image(obj);

        /* Is object outside of the line we're drawing? */
        if (x < obj_x || x >= obj_x + img->hdr.width ||
            y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
            continue;
        }

        /* The x offset of drawing is fixed. */
        xoff = x - obj_x;

        /*
         * The y offsets depend on whether the object starts below or
         * above the starting point for the line being drawn.
         */
        if (y <= obj_y) {
            idx = obj_y - y;
            imgy = 0;
        }
        else {
            idx = 0;
            imgy = y - obj_y;
        }

        /* Copy the object's pixel data. */
        for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
            pixel = img->img[xoff + img->hdr.width * imgy];

            /* Don't copy transparent pixels. */
            if (OBJ_CLR_TRANSP != pixel) {
                buf[idx] = pixel;
            }
        }
    }
}


/*
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t image_height(const image_t* im) {
    return im->hdr.height;
}


/*
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t image_width(const image_t* im) {
    return im->hdr.width;
}

/*
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t photo_height(const photo_t* p) {
    return p->hdr.height;
}


/*
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t photo_width(const photo_t* p) {
    return p->hdr.width;
}


/*
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void prep_room(const room_t* r) {
    /* Record the current room. */
    cur_room = r;
    /*Get a pointer to the current room and use it to set the pallette*/
    photo_t * photo_struct = room_photo(r);
    /*Write the palette data to video memory*/
    set_palette((unsigned char *)photo_struct->palette);
}


/*
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t* read_obj_image(const char* fname) {
    FILE*    in;        /* input file               */
    image_t* img = NULL;    /* image structure          */
    uint16_t x;            /* index over image columns */
    uint16_t y;            /* index over image rows    */
    uint8_t  pixel;        /* one pixel from the file  */

    /*
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen(fname, "r+b")) ||
        NULL == (img = malloc(sizeof (*img))) ||
        NULL != (img->img = NULL) || /* false clause for initialization */
        1 != fread(&img->hdr, sizeof (img->hdr), 1, in) ||
        MAX_OBJECT_WIDTH < img->hdr.width ||
        MAX_OBJECT_HEIGHT < img->hdr.height ||
        NULL == (img->img = malloc
        (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
        if (NULL != img) {
            if (NULL != img->img) {
                free(img->img);
            }
            free(img);
        }
        if (NULL != in) {
            (void)fclose(in);
        }
        return NULL;
    }

    /*
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order(top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

        /* Loop over columns from left to right. */
        for (x = 0; img->hdr.width > x; x++) {

            /*
             * Try to read one 8-bit pixel.  On failure, clean up and
             * return NULL.
             */
            if (1 != fread(&pixel, sizeof (pixel), 1, in)) {
                free(img->img);
                free(img);
                (void)fclose(in);
                return NULL;
            }

            /* Store the pixel in the image data. */
            img->img[img->hdr.width * y + x] = pixel;
        }
    }

    /* All done.  Return success. */
    (void)fclose(in);
    return img;
}


/*
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t* read_photo(const char* fname) {
    FILE*    in;    /* input file               */
    photo_t* p = NULL;    /* photo structure          */
    uint16_t x;        /* index over image columns */
    uint16_t y;        /* index over image rows    */
    uint16_t pixel;    /* one pixel from the file  */

    /*
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen(fname, "r+b")) ||
        NULL == (p = malloc(sizeof (*p))) ||
        NULL != (p->img = NULL) || /* false clause for initialization */
        1 != fread(&p->hdr, sizeof (p->hdr), 1, in) ||
        MAX_PHOTO_WIDTH < p->hdr.width ||
        MAX_PHOTO_HEIGHT < p->hdr.height ||
        NULL == (p->img = malloc
        (p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
        if (NULL != p) {
            if (NULL != p->img) {
                free(p->img);
            }
            free(p);
        }
        if (NULL != in) {
            (void)fclose(in);
        }
        return NULL;
    }

    /*Generate an array to store the raw color data*/
    uint16_t * raw_color_data = malloc(sizeof(pixel) * p->hdr.height * p->hdr.width);

    /*
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order(top to bottom).
     */
    for (y = p->hdr.height; y-- > 0; ) {

        /* Loop over columns from left to right. */
        for (x = 0; p->hdr.width > x; x++) {

            /*
             * Try to read one 16-bit pixel.  On failure, clean up and
             * return NULL.
             */
            if (1 != fread(&pixel, sizeof (pixel), 1, in)) {
                free(p->img);
                free(p);
                (void)fclose(in);
                return NULL;
            }

            /*save the raw color data*/
            raw_color_data[p->hdr.width * y + x] = pixel;

            /*
             * 16-bit pixel is coded as 5:6:5 RGB(5 bits red, 6 bits green,
             * and 6 bits blue).  We change to 2:2:2, which we've set for the
             * game objects.  You need to use the other 192 palette colors
             * to specialize the appearance of each photo.
             *
             * In this code, you need to calculate the p->palette values,
             * which encode 6-bit RGB as arrays of three uint8_t's.  When
             * the game puts up a photo, you should then change the palette
             * to match the colors needed for that photo.
             */
            p->img[p->hdr.width * y + x] = (((pixel >> 14) << 4) | (((pixel >> 9) & 0x3) << 2) | ((pixel >> 3) & 0x3));
        }
    }

    /*Generate the color pallette and free the raw color data*/
    gen_color_pallette(raw_color_data, p);
    /*Free the raw color data array because we no longer need it*/
    free(raw_color_data);

    /* All done.  Return success. */
    (void)fclose(in);
    return p;
}

/* inverse_cmp is a comparison function used by qsort
 * later in the program.it returns -1 when a > b and 1 otherwise.
 * It is inverted so that when the array is sorted, the largest
 * elements occur first.
 */
int inverse_cmp(const void * a, const void * b){
      return (*(int *)b - *(int  *)a);
}

/*
 * level4_index takes as argument a 5:6:5 RGB color argument andb
 * returns a 4:4:4 RGB color index, which is used to index the level
 * four octree. Only the most significant bytes are taken from the
 * original "color_data"
 */
uint16_t level4_index(uint16_t color_data){
      unsigned short R, G, B;
      /*Grab the four most significant bits*/
      R = (color_data & 0xF000) >> 12;
      G = (color_data & 0x0780) >> 7;
      B = (color_data & 0x001E) >> 1;
      return (uint16_t)((R << 8) | (G << 4) | B);
}

/*
 * level2_index takes as argument a 5:6:5 RGB color argument andb
 * returns a 2:2:2 RGB color index, which is used to index the level
 * two octree. Only the most significant bytes are taken from the
 * original "color_data"
 */
uint16_t level2_index(uint16_t color_data){
      unsigned short R, G, B;
      /*Grab the two most significant bits*/
      R = (color_data & 0xC000) >> 14;
      G = (color_data & 0x0600) >> 9;
      B = (color_data & 0x0018) >> 3;
      return (uint16_t)((R << 4) | (G << 2) | B);
}

/*
 * gen_color_pallette(uint16_t * raw_color_data, photo_t * p)
 * DESCRIPTION:         takes the unedited information about a photo
 *                      and creates a new palette for the image and
 *                      saves the associated 256 color array in the
 *                      p photo_t structure
 * INPUTS:              raw_color_data - The unedited information
 *                      from the photo containing the 5:6:5 RGB color values
 * OUTPUTS:             none
 * SIDE EFFECTS:        updates the palette and img fields in the photo
 *                      to the correct values
 */
void gen_color_pallette(uint16_t * raw_color_data, photo_t * p){
      long level2[LEVEL2_SIZE];                          /*count of entries in the level 2 octree*/
      long level4[LEVEL4_SIZE];                        /*count of entries in the level 4 octree*/
      unsigned long level2average[LEVEL2_SIZE][COLOR_COUNT];       /*Average values of the level 2 octree*/
      unsigned long level4average[LEVEL4_SIZE][COLOR_COUNT];     /*Average values of the level 4 octree*/
      int level4index, level2index;             /*Indices into the octrees*/
      unsigned long level4count, level2count;   /*Number of elements at associated value of octree*/
      int i, j;         /*Used for "for" loops*/
      long R, G, B;     /*Hold the raw RGB values*/

      /*
       * The way sorting works for my implementation is relatively simple.
       * the low bits represent either the associated 2:2:2 or 4:4:4 RGB
       * values for a given level of octree. The high bits contain the number
       * of occurences of these values in the original photo. This allows us
       * to sort the list as intended while also keeping track of the associated
       * RGB values
       */

       /*Fill the octrees with the associated RGB values */

      for(i = 0; i < LEVEL2_SIZE; i++){
            level2[i] = i;
      }
      for(i = 0; i < LEVEL4_SIZE; i++){
            level4[i] = i;
      }

      /*Count the number of occurences for each entry of the octree levels while also keeping a
       *rolling average count of the RGB values
       */
      for(i = 0; i < (p->hdr.height * p->hdr.width); i++){
            /*Grab the most significant bits so we can index into our octrees*/
            level4index = level4_index(raw_color_data[i]);
            level2index = level2_index(raw_color_data[i]);
            /*Get the number of occurences at each entry of the octrees*/
            level4count = level4[level4index] >> LEVEL4COUNT_OFFSET;
            level2count = level2[level2index] >> LEVEL4COUNT_OFFSET;

            /*Get the raw RGB values from the pixel grabbed from the image*/
            R = ((raw_color_data[i] & RAW_RED_MASK) >> RAW_RED_OFFSET) << 1;
            G = ((raw_color_data[i] & RAW_GREEN_MASK) >> RAW_GREEN_OFFSET) << 0;
            B = ((raw_color_data[i] & RAW_BLUE_MASK) >> RAW_BLUE_OFFSET) << 1;

            /*For simplicity, rolling averages are used to keep code straightforward.
            * New Average = (old average * old count) + New entry / (old count + 1)
            */

            /*Calculate and update the averages at level 4*/
            level4average[level4index][RED] = ((level4average[level4index][RED]*level4count) + R)/(level4count + 1);
            level4average[level4index][GRN] = ((level4average[level4index][GRN]*level4count) + G)/(level4count + 1);
            level4average[level4index][BLU] = ((level4average[level4index][BLU]*level4count) + B)/(level4count + 1);

            /*Calculate and update the averages at level 2*/
            level2average[level2index][RED] = ((level2average[level2index][RED]*level2count) + R)/(level2count + 1);
            level2average[level2index][GRN] = ((level2average[level2index][GRN]*level2count) + G)/(level2count + 1);
            level2average[level2index][BLU] = ((level2average[level2index][BLU]*level2count) + B)/(level2count + 1);

            /*Increment the counts upate the number of occurences in each list*/
            level4count++;
            level4[level4index] &= LOW_12_BITMASK;
            level4[level4index] |= (level4count << LEVEL4COUNT_OFFSET);

            level2count++;
            level2[level2index] &= LOW_6_BITMASK;
            level2[level2index] |= (level2count << LEVEL2COUNT_OFFSET);

      }

      /*Sort the level 4 octree*/
      qsort(level4, LEVEL4_SIZE, sizeof(long), inverse_cmp);

      /*Write the 8-bit data mappings into the p->img[] array
      * - Level four mappings are from 64 + 0 to 64 + 128
      * - level two mappings are from 64 + 128 + 0 to 64 + 128 + 64
      * The additional 64 is for the first 64 values in vga vidmem
      * reserved for the status bar and objects
      */
      for(i = 0; i < (p->hdr.height * p->hdr.width); i++){
            /*By default, write the associated level 2 entry into memory*/
            p->img[i] = VIDMEM_PAL_OFFSET + LEVEL2_VIDMEM_OFFSET + level2_index(raw_color_data[i]);
            /*If a level 4 entry for the pixel exists, overwrite that*/
            for(j = 0; j < LEVEL4_COLORS_USED; j++){
                  if(level4_index(raw_color_data[i]) == (level4[j] & LOW_12_BITMASK)){
                        p->img[i] = VIDMEM_PAL_OFFSET + j;
                  }
            }
      }

      /*Write the level 4 octree into the palette (level 4 goes from 0 to 127)*/
      for(i = 0; i < 128; i++){
            p->palette[i][RED] = level4average[level4[i]&LOW_12_BITMASK][RED];
            p->palette[i][GRN] = level4average[level4[i]&LOW_12_BITMASK][GRN];
            p->palette[i][BLU] = level4average[level4[i]&LOW_12_BITMASK][BLU];
      }

      /*Write the level 2 octree into the palette (level 2 goes from 128 to 191)*/
      for(i = 0; i < 64; i++){
            p->palette[i + LEVEL2_VIDMEM_OFFSET][RED] = level2average[i][RED];
            p->palette[i + LEVEL2_VIDMEM_OFFSET][GRN] = level2average[i][GRN];
            p->palette[i + LEVEL2_VIDMEM_OFFSET][BLU] = level2average[i][BLU];
      }

      return;
}
