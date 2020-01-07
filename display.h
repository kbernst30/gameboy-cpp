#ifndef __DISPLAY_H_INCLUDED__
#define __DISPLAY_H_INCLUDED__

#include "mmu.h"
#include "utils.h"

class Display {

    public:
        Display(Mmu *_mmu) : mmu(_mmu) {};

        void drawScanline();

        Color getPixel(int x, int y);

    private:
        Mmu *mmu;

        // The screen has width * height (color is RGB)
        Color screen[SCREEN_WIDTH][SCREEN_HEIGHT];

        void renderBackground(Byte lcdControl);
        void renderSprites(Byte lcdControl);

        Color getColor(int colorData, Word paletteAddr);

};

#endif