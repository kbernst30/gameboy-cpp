#ifndef __CPU_H_INCLUDED__
#define __CPU_H_INCLUDED__

#include "mmu.h"
#include "utils.h"

class Cpu {

    public:
        Cpu(Mmu *_mmu) : mmu(_mmu) {};

        // Should return number of clock cycles
        // needed for the executing operation
        int execute();

        // Reset CPU to initial state
        void reset();

    private:
        Mmu *mmu;

        // There are 8 8-bit registers in the Gameboy
        // A, B, C, D, E, F, H and L. They are usually
        // referred to in pairs, so we represent them as such
        Register af;
        Register bc;
        Register de;
        Register hl;

        // The Program Counter is 2 bytes and points to address
        // in memory of next operation to execute
        Word progamCounter;

        // The Stack Pointer is 2 bytes and points to the top of
        // the stack in memory. We will use the Register type
        // because we often need to get hi and lo byte from the
        // stack
        Register stackPointer;

};

#endif
