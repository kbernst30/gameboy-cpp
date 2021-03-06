#ifndef __UTILS_H_INCLUDED__
#define __UTILS_H_INCLUDED__

typedef unsigned char Byte;
typedef char SignedByte;
typedef unsigned short Word;
typedef signed short SignedWord;

// This union represents a register pair, which we can set
// in its entirety or ask for the hi byte, or lo byte.
// For example, register pair BC has a hi byte (B) and lo
// byte (C). Lo is declared first in the struct because
// the Gameboy is little endian
union Register {
    Word reg;
    struct {
        Byte lo;
        Byte hi;
    } parts;
};

// Color struct to hold RGB values
struct Color {
    int red;
    int green;
    int blue;
};

const int CLOCK_SPEED = 4194304; // cycles/sec
const double FRAMES_PER_SECOND = 59.73;
const int MAX_CYCLES_PER_FRAME = 70221; // Math.floor(4194304 / 59.73)

const int CARTRIDGE_SIZE = 0x200000;
const int MEMORY_ROM_SIZE = 0x8000;
const int MEMORY_SIZE = 0x10000;
const int SCREEN_WIDTH = 160;
const int SCREEN_HEIGHT = 144;
const int MAX_SCANLINES = 153; // There are 9 invisible scanlines (past 144)

// Flag Bits in Register F
const int ZERO_BIT = 7;
const int SUBTRACT_BIT = 6;
const int HALF_CARRY_BIT = 5;
const int CARRY_BIT = 4;

// Banking
const int ROM_BANKING_MODE_ADDR = 0x147;
const int RAM_BANK_COUNT_ADDR = 0x148;
const int MAXIMUM_RAM_BANKS = 4;
const int RAM_BANK_SIZE = 0x2000; // In bytes

// Timers
const int DIVIDER_REGISTER_ADDR = 0xFF04;
const int TIMER_ADDR = 0xFF05;
const int TIMER_MODULATOR_ADDR = 0xFF06; // The value at this address is what the timer is set to upon overflow

// Timer Controller is 3-bit that controls timer and specifies frequency.
// The 1st 2 bits describe frequency. Here is the mapping:
// 00: 4096 Hz
// 01: 262144 Hz
// 10: 65536 Hz
// 11: 16384 Hz
//
// The third bit specifies if the timer is enabled (1) or disabled (0)
// This is the memory address that the controller is stored at
const int TIMER_CONTROLLER_ADDR = 0xFF07;

// Interrupts
// There are 4 types of interrupts that can occur and the following are the bits
// that are set in the enabled register and request register when they occur
// Note: the lower the bit, the higher priority of the interrupt
// Bit 0: V-Blank Interupt
// Bit 1: LCD Interupt
// Bit 2: Timer Interupt
// Bit 4: Joypad Interupt
const int INTERRUPT_ENABLED_REGISTER = 0xFFFF;
const int INTERRUPT_REQUEST_ADDR = 0xFF0F;

// LCD and Graphics
const int CURRENT_SCANLINE_ADDR = 0xFF44;

// This is the LCD control register
// Bit 7 specifies if the LCD is currently enabled
// Bit 6 is the Window Tile Map Display Select
//   i.e. Where to read the tile identity number to draw onto the window
//   0 = 0x9800 - 0x9BFF, 1 = 0x9C00 - 0x9FFF
// Bit 5 specifies if the Window is currently enabled
// Bit 4 is the Background and Window Tile Data Select
//   i.e. Using the tile identity number, we can get the data of the tile
//   that needs to be drawn. If this value is 0, the tile identity number is
//   a signed byte, not unsigned
//   0 = 0x8800 - 0x97FF, 1 = 0x8000 - 0x8FFF
// Bit 3 is the Background Tile Map Display Select
//   i.e. the same as Bit 6 but for the background instead of Window
//   0 = 0x9800 - 0x9BFF, 1 = 0x9C00 - 0x9FFF
// Bit 2 is the size of the sprites that need to be drawn
//   0 = 8x8, 1 = 8x16
// Bit 1 specifies if the Sprites are currently enabled
// Bit 0 specified if the Background is currently enabled
const int LCD_CONTROL_ADDR = 0xFF40;

// This is where the status of the LCD is stored
// The LCD has 4 modes and Bits 1 and 0 of the LCD status
// specify which mode we are currently in:
// 00: H-Blank Mode
// 01: V-Blank mode
// 10: Searching Sprites Atts
// 11: Transferring Data to LCD driver
// Bits 3, 4, and 5 specify if interrupts are enabled for the
// LCD mode we are entering. Basically, this means if we move
// into a specific LCD mode and the interrupt is enabled, we
// will request an LCD interrupt. The bits are as follows:
// Bit 3: Mode 0 (H-Blank) interrupt enabled
// Bit 4: Mode 1 (V-Blank) interrupt enabled
// Bit 5: Mode 2 (Searching Sprites) interrupt enabled
// Bit 2 is the "Coincidence" flag and is set to 1 if the current scanline
// register (0xFF44) is the same value as the value in 0xFF45, otherwise 0
// Bit 6 is the "Coincidence Interrupt Enabled" flag. If the coincidence
// flag is set and so is this bit, then we will request a LCD interrupt
const int LCD_STATUS_ADDR = 0xFF41;

// The screen is only 160x144 but there is 256x256 bytes of screen data
// We only need to draw what should be visible. The following specify
// where to start drawing the background and window
const int SCROLL_Y_ADDR = 0xFF42; // the Y pos of the background to start drawing the viewing area
const int SCROLL_X_ADDR = 0xFF43; // the X pos of the background to start drawing the viewing area
const int WINDOW_Y_ADDR = 0xFF4A; // the Y pos of the viewing area to start drawing the window
const int WINDOW_X_ADDR = 0xFF4B; // the X pos-7 of the viewing area to start drawing the window

// This is where the "palettes" are stored and we use it to determine a color based on 2 bits
// of data (i.e. 10 or 11) will be cross referenced with the paettes to get the correct color
// This works by taking those two bits and looking at the corresponding two bits in the palette.
// These mapping are as follows:
//  11 - Bits 7 and 6
//  10 - Bits 5 and 4
//  01 - Bits 3 and 2
//  00 - Bits 1 and 0
// The colors for the Gameboy are simple (grey scale) and the bits in the palette we are examining
// specify the color. The colors are as follows:
//  00 = White
//  01 = Light Gray
//  10 = Dark Gray
//  11 = Black
const int BACKGROUND_COLOR_PALETTE_ADDR = 0xFF47;
const int SPRITE_COLOR_PALETTE_1_ADDR = 0xFF48;
const int SPRITE_COLOR_PALETTE_2_ADDR = 0xFF49;

// This is the starting point of the sprite attribute table. We need to look at 4 bytes per sprite in
// the memory range starting from here
const int SPRITE_ATTRIBUTE_TABLE_ADDR = 0xFE00;

// The joypad register is found here
// Bits 7 and 6 are not used
// Bit 5 specifies if we are checking standard buttons (i.e. A, B)
// Bit 4 specifies if we are checking direction buttons
// Bit 3 specified if Down or Start is pressed (0 is pressed)
// Bit 2 specified if Up or Select is pressed (0 is pressed)
// Bit 1 specified if Left or B is pressed (0 is pressed)
// Bit 0 specified if Right or A is pressed (0 is pressed)
const int JOYPAD_REGISTER_ADDR = 0xFF00;

/* -------------------------Util Functions--------------------------- */

template <typename T>
bool isBitSet(T data, int position)
{
    // Return true if bit at position is
    // set in data, false otherwise
    T test = 1 << position;
    return (data & test) > 0 ? true : false;
}

template <typename T>
void setBit(T *data, int position)
{
    // Set bit at position in data
    T setter = 1 << position;
    *data |= setter;
}

template <typename T>
void resetBit(T *data, int position)
{
    // Unset bit at position in data
    T setter = 1 << position;
    setter = ~setter; // Bit wise negate to get a 0 in the appropriate pos
    *data &= setter;
}

template <typename T>
int getBitVal(T data, int position)
{
    T test = 1 << position;
    return (data & test) > 0 ? 1 : 0;
}

#endif