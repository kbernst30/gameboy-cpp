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
        this->renderBackground();
    }
    
    if (isBitSet(lcdControl, 1))
    {
        // If Bit 1 is set, that means the sprites are enabled,
        // and we should draw them
        this->renderSprites();
    }
}

void Display::renderBackground()
{

}

void Display::renderSprites()
{

}
