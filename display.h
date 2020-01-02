#ifndef __DISPLAY_H_INCLUDED__
#define __DISPLAY_H_INCLUDED__

#include "mmu.h"
#include "utils.h"

class Display {

    public:
        Display(Mmu *_mmu) : mmu(_mmu) {};

        void drawScanline();

    private:
        Mmu *mmu;

        // The screen has width * height * color (color is RGB, hence size 3)
        Byte screen[SCREEN_WIDTH][SCREEN_HEIGHT][3];

        void renderBackground(Byte lcdControl);
        void renderSprites(Byte lcdControl);

        Color getColor(int colorData, Word paletteAddr);

};

#endif