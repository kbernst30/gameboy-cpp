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

const int CLOCK_SPEED = 4194304; // cycles/sec
const int FRAMES_PER_SECOND = 60;
const int MAX_CYCLES_PER_FRAME = 69905; // Math.floor(4194304 / 60)

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

/* -------------------------Util Functions--------------------------- */

template <typename T>
bool isBitSet(T data, int position)
{
    // Return true if bit at position is
    // set in data, false otherwise
    T test = 1 << position;
    return data & test ? true : false;
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

#endif