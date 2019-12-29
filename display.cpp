#include "display.h"
#include "mmu.h"
#include "utils.h"

void Display::drawScanline()
{
    // Get the value of the LCD control register
    Byte lcdControl = this->mmu->readMemory(LCD_CONTROL_ADDR);

    if (isBitSet(lcdControl, 0))
    {
        // If Bit 0 is set, that means the background is enabled, 
        // and we should draw it
        this->renderBackground(lcdControl);
    }
    
    if (isBitSet(lcdControl, 1))
    {
        // If Bit 1 is set, that means the sprites are enabled,
        // and we should draw them
        this->renderSprites();
    }
}

void Display::renderBackground(Byte lcdControl)
{
    Word tileData = 0;
    Word backgroundMemory = 0;
    bool isUnsigned = true;

    Byte currentScanline = this->mmu->readMemory(CURRENT_SCANLINE_ADDR);

    // We need to figure out where to draw the visual area of the 
    // background as well as the Window
    Byte scrollX = this->mmu->readMemory(SCROLL_X_ADDR);
    Byte scrollY = this->mmu->readMemory(SCROLL_Y_ADDR);
    Byte windowX = this->mmu->readMemory(WINDOW_X_ADDR);
    Byte windowY = this->mmu->readMemory(WINDOW_Y_ADDR);

    // We need to determine if the current scanline we are drawing
    // is part of the window as opposed to the background
    // The window sits above the background, but below any sprites
    bool usingWindow = false;
    if (isBitSet(lcdControl, 5))
    {
        // Bit 5 of lcdControl determines whether or not the window
        // is enabled
        if (windowY <= currentScanline)
        {
            // If the window is within the range of the current scanline
            usingWindow = true;
        }
    }

    // Let's determine which tile data we are using
    // Bit 4 of LCD control will tell us where this is
    // If it is 0, we will look in region starting at 0x8800
    // If it is 1, we will look in region starting at 0x8000
    if (isBitSet(lcdControl, 4))
    {
        tileData = 0x8000;
    }
    else
    {
        tileData = 0x8800;

        // We are using a signed byte for the tile identifier number
        // if we are in this region
        isUnsigned = false;
    }

    // The tile identifier number is in one of two regions. Based on if we
    // are drawing the window or not, we should test but 6 (for window) or
    // 3 (for background) to determine which region we will look in
    if (usingWindow)
    {
        if (isBitSet(lcdControl, 6))
        {
            backgroundMemory = 0x9C00;
        }
        else
        {
            backgroundMemory = 0x9800;
        }
    }
    else
    {
        if (isBitSet(lcdControl, 3))
        {
            backgroundMemory = 0x9C00;
        }
        else
        {
            backgroundMemory = 0x9800;
        }
    }

    // Each tile is 8x8 pixels. Each line of the tile (i.e. 8 pixels)
    // needs two bytes of data in memory to determine the proper color
    // There are 32 vertical tiles (each 8x8) that fill up the whole
    // possible range of 256 pixel height of the screen. Using data
    // above, we can determine WHICH tile we are drawing

    Byte yPos = 0;
    if (usingWindow)
    {
        yPos = currentScanline - windowY;
    }
    else
    {
        yPos = scrollY + currentScanline;
    }

    Word tileRow = ((Byte) (yPos / 8)) * 32;
    
    // Each scanline draws 160 pixels horizontally, so we need to draw
    // them each. Remember, each tile is 8 pixels wide so we should
    // basically draw 20 tiles for each scanline
    for (int i = 0; i < SCREEN_WIDTH; i++)
    {
        // We need to get the appropriate xPos to draw the tile at
        // based on values above
        Byte xPos = i + scrollX;

        // If we are using the window and the pixel is within
        // the range of the window
        if (usingWindow)
        {
            if (i >= windowX)
            {
                xPos = i - windowX;
            }
        }

        // Deterime which horizontal tile we are currently drawing
        Word tileCol = xPos / 8;

        // We need the tile identification number. It might be signed
        Word tileAddress = backgroundMemory + tileRow + tileCol;
        SignedWord tileIdentificationNumber;
        if (isUnsigned)
        {
            tileIdentificationNumber = this->mmu->readMemory(tileAddress);
        }
        else
        {
            tileData = (SignedByte) this->mmu->readMemory(tileAddress);
        }

        // 
        
    }
    
}

void Display::renderSprites()
{

}
