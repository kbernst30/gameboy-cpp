#ifndef __DISPLAY_H_INCLUDED__
#define __DISPLAY_H_INCLUDED__

#include "utils.h"

class Display {

    private:
        // The screen has width * height * color (color is RGB, hence size 3)
        Byte screen[SCREEN_WIDTH][SCREEN_HEIGHT][3];

};

#endif