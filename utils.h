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

/* -------------------------Util Functions--------------------------- */

template <typename T>
bool isBitSet(T data, int position)
{
    // Return true if bit at position is
    // set in data, false otherwise
    T test = 1 << position;
    return data & test ? true : false;
}

#endif