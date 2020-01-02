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
        this->renderSprites(lcdControl);
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
            tileIdentificationNumber = (SignedByte) this->mmu->readMemory(tileAddress);
        }

        // Using the tileIdentificationNumber, we should be able to determine where in memory
        // the tile should be. If it is signed value, then we need to properly determine the
        // position using a signed offset
        Word tileLocation = tileData;
        if (isUnsigned)
        {
            tileLocation += tileIdentificationNumber * 16;
        }
        else
        {
            tileLocation += (tileIdentificationNumber + 128) * 16;
        }

        // There are 8 pixels in the tile height, so we need to get the appropriate line
        Byte line = yPos % 8;

        // This is to get the proper address offset from the tile location. Each tile takes
        // up two bytes in memory so we need to account for this
        line *= 2;

        // This is the tile data. Occupying the two bytes in memory
        Byte data1 = this->mmu->readMemory(tileLocation + line);
        Byte data2 = this->mmu->readMemory(tileLocation + line + 1);

        // We now have the two bytes that specify the line of the tile
        // To get the pixels (i.e. the right color) we need to combine them
        // This works like the following example (note endianness):
        //
        // Pixel #:   0  1  2  3  4  5  6  7
        // data2 bit: 1  0  1  0  1  1  1  0
        // data1 bit: 0  0  1  1  0  1  0  1
        //
        // Pixel 0 colour id: 10
        // Pixel 1 colour id: 00
        // Pixel 2 colour id: 11
        // Pixel 3 colour id: 01
        // Pixel 4 colour id: 10
        // Pixel 5 colour id: 11
        // Pixel 6 colour id: 10
        // Pixel 7 colour id: 01

        // This is the bit of the data bytes we are looking at
        int colorBit = ((xPos % 8) - 7) * -1;

        // Now we need to combine the bits of the data bytes as specified above
        int colorData = getBitVal(data2, colorBit);
        colorData <<= 1;
        colorData |= getBitVal(data1, colorBit);

        Color color = getColor(colorData, BACKGROUND_COLOR_PALETTE_ADDR);

        if (currentScanline < 0 || currentScanline >= SCREEN_HEIGHT || i < 0 || i >= SCREEN_WIDTH)
        {
            // If we are outside the visible screen do not set data in the screen data as it will
            // error
            continue;
        }

        // Set the proper pixel in the screen data
        screen[i][currentScanline][0] = color.red;
        screen[i][currentScanline][1] = color.green;
        screen[i][currentScanline][2] = color.blue;
    }

}

void Display::renderSprites(Byte lcdControl)
{
    // Unlike background, tile identifier for sprites is ALWAYS unsigned
    // and tile data is always in memory 0x8000 - 0x8FFFF. There are 40 tiles
    // here and we need to look at them all and find where they should be rendered
    // There is a sprite attribute table in memory 0xFE00 - 0xFE9F. Here, we can
    // look at 4 bytes per sprite. These bytes are as follows:
    //  Byte 0: Sprite Y position - 16
    //  Byte 1: Sprite X position - 8
    //  Byte 2: Pattern number (i.e. sprite identifier to look up in memory)
    //  Byte 3: Attributes
    //
    // The attributes of the sprite (byte 3) breakdown as follows:
    //  Bit 7: Sprite to Background priority - render above or below background
    //  Bit 6: Y Flip
    //  Bit 5: X Flip
    //  Bit 4: Palette Number - More than one color palette for sprites
    //  Bit 3-0: Not used

    // A tall sprite is 8x16, whereas a small sprite is 8x8
    bool isTallSprite = isBitSet(lcdControl, 2);

    // There are 40 sprites, so check them all
    for (int sprite = 0; sprite < 40; sprite++)
    {
        int memoryIdx = sprite * 4; // 4 bytes per sprite, so get proper starting point
        Byte yPosition = this->mmu->readMemory(SPRITE_ATTRIBUTE_TABLE_ADDR + memoryIdx) - 16;
        Byte xPosition = this->mmu->readMemory(SPRITE_ATTRIBUTE_TABLE_ADDR + memoryIdx + 1) - 8;
        Byte tileLocation = this->mmu->readMemory(SPRITE_ATTRIBUTE_TABLE_ADDR + memoryIdx + 2);
        Byte attributes = this->mmu->readMemory(SPRITE_ATTRIBUTE_TABLE_ADDR + memoryIdx + 3);

        bool yFlip = isBitSet(attributes, 6);
        bool xFlip = isBitSet(attributes, 5);

        int currentScanline = this->mmu->readMemory(CURRENT_SCANLINE_ADDR);

        // We should only draw this sprite if it intercepts with the scanline that we are currently drawing
        int spriteHeight = isTallSprite ? 16 : 8;
        if (currentScanline >= yPosition && currentScanline < yPosition + spriteHeight)
        {
            int line = currentScanline - yPosition;

            // If we have Y flip, read the sprite in backwards to achieve the flip
            if (yFlip)
            {
                line = (line - spriteHeight) * -1;
            }

            // Remember each tile (sprite or background) has two bytes of memory
            // So do this to get the appropriate address
            line *= 2;

            // Get the address to find the sprite data
            Word dataAddress = 0x8000 + (tileLocation * 16) + line;

            // This is the tile data. Occupying the two bytes in memory
            Byte data1 = this->mmu->readMemory(dataAddress + line);
            Byte data2 = this->mmu->readMemory(dataAddress + line + 1);

            // With background we had the pixel as we were looping through every horizontal pixel in
            // the scanline. Here we need to get the horizontal pixel from the sprite. Check them
            // right to left to read color data properly (similar to background)
            for (int tilePixel = 7; tilePixel >= 0; tilePixel--)
            {
                int colorBit = tilePixel;

                // If we have X Flip, read the sprite in backwards to achieve the flip
                if (xFlip)
                {
                    colorBit = (colorBit - 7) * -1;
                }

                // Now we need to combine the bits of the data bytes as specified above
                int colorData = getBitVal(data2, colorBit);
                colorData <<= 1;
                colorData |= getBitVal(data1, colorBit);

                Word paletteAddr = isBitSet(attributes, 4) ? SPRITE_COLOR_PALETTE_2_ADDR : SPRITE_COLOR_PALETTE_1_ADDR;
                Color color = getColor(colorData, paletteAddr);

                // Sprite don't really have the "WHITE" color - they are transparent here, so we shouldn't set the data
                // at all
                if (color.red == 0xFF && color.green == 0XFF && color.blue == 0xFF)
                {
                    continue;
                }

                // TODO check background priority???

                int xPixel = 0 - tilePixel;
                xPixel += 7;
                int pixel = xPosition + xPixel;

                if (currentScanline < 0 || currentScanline >= SCREEN_HEIGHT || pixel < 0 || pixel >= SCREEN_WIDTH)
                {
                    // If we are outside the visible screen do not set data in the screen data as it will
                    // error
                    continue;
                }

                // Set the proper pixel in the screen data
                screen[pixel][currentScanline][0] = color.red;
                screen[pixel][currentScanline][1] = color.green;
                screen[pixel][currentScanline][2] = color.blue;
            }
        }
    }
}

Color Display::getColor(int colorData, Word paletteAddr)
{
    Color color;
    int colorBits;

    Byte palette = this->mmu->readMemory(paletteAddr);

    // Get the appropriate color bits from the palette.
    // The data maps to the bits in the palette as follows:
    //  11 - Bits 7 and 6
    //  10 - Bits 5 and 4
    //  01 - Bits 3 and 2
    //  00 - Bits 1 and 0
    switch (colorData)
    {
        case 11: colorBits = (getBitVal(palette, 7) << 1) | getBitVal(palette, 6); break;
        case 10: colorBits = (getBitVal(palette, 5) << 1) | getBitVal(palette, 4); break;
        case 01: colorBits = (getBitVal(palette, 3) << 1) | getBitVal(palette, 2); break;
        case 00: colorBits = (getBitVal(palette, 1) << 1) | getBitVal(palette, 0); break;
    }

    // We will have two bits now that map to colors as follows:
    //  00 = White = 0xFFFFFF
    //  01 = Light Gray = 0xCCCCCC
    //  10 = Dark Gray = 0x777777
    //  11 = Black = 0x000000
    switch (colorBits)
    {
        case 00: color.red = 0xFF; color.green = 0xFF; color.blue = 0xFF; break;
        case 01: color.red = 0xCC; color.green = 0xCC; color.blue = 0xCC; break;;
        case 10: color.red = 0x77; color.green = 0x77; color.blue = 0x77; break;;
        case 11: color.red = 0x00; color.green = 0x00; color.blue = 0x00; break;;
    }

    return color;
}
