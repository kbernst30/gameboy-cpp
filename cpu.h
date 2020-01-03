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

        // Set value of the interrupt request address to include requested interrupt
        void requestInterrupt(int bit);

        bool isInterruptMaster();
        void setInterruptMaster(bool val);

        void serviceInterrupt(int interrupt);

    private:
        Mmu *mmu;

        void pushWordTostack(Word word);
        Word popWordFromStack();

        int doOpcode(Byte opcode);
        int doExtendedOpcode(Byte opcode);

        // Op helpers
        Word getNextWord();
        Byte getNextByte();
        void do8BitLoad(Byte *reg);
        void do8BitLoadToMemory(Word address);
        void do16BitLoad(Word *reg);
        void do8BitRegisterAdd(Byte *reg, Byte value, bool useCarry=false);
        void do16BitRegisterAdd(Word *reg, Byte value);
        void do8BitRegisterSub(Byte *reg, Byte value, bool useCarry=false);
        void do8BitRegisterAnd(Byte *reg, Byte value);
        void do8BitRegisterOr(Byte *reg, Byte value);
        void do8BitRegisterXor(Byte *reg, Byte value);
        void do8BitRegisterCompare(Byte source, Byte value);
        void do8BitRegisterIncrement(Byte *reg);
        void do8BitRegisterDecrement(Byte *reg);
        void do8BitRegisterSwap(Byte *reg);
        void do8BitRegisterRotateLeft(Byte *reg, bool throughCarry=false);
        void do8BitRegisterShiftLeft(Byte *reg);
        void do8BitRegisterRotateRight(Byte *reg, bool throughCarry=false);
        void do8BitRegisterShiftRight(Byte *reg, bool maintainMsb=false);

        // There are 8 8-bit registers in the Gameboy
        // A, B, C, D, E, F, H and L. They are usually
        // referred to in pairs, so we represent them as such
        Register af;
        Register bc;
        Register de;
        Register hl;

        // The Program Counter is 2 bytes and points to address
        // in memory of next operation to execute
        Word programCounter;

        // The Stack Pointer is 2 bytes and points to the top of
        // the stack in memory. We will use the Register type
        // because we often need to get hi and lo byte from the
        // stack
        Register stackPointer;

        // The master interrupt enabled switch
        // This is stored here as the CPU will need direct access to its control
        bool interruptMaster = true;

        // Used by instructions DI and EI - we need to know if have a pending
        // interrupt enable/disable
        bool willDisableInterrupts = false;
        bool willEnableInterrupts = false;

        // Specify if the CPU is halted or not
        bool halted = false;

};

#endif
