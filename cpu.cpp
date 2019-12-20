#include "cpu.h"
#include "utils.h"

int Cpu::execute() {
    return 123;
}

void Cpu::reset() {
    // This is the initial state of the CPU registers
    // program counter, and stack pointer. The following
    // is documented in Gameboy architecture
    this->af.reg = 0x01B0;
    this->bc.reg = 0x0013;
    this->de.reg = 0x00D8;
    this->hl.reg = 0x014D;

    this->programCounter = 0x100;
    this->stackPointer.reg = 0xFFFE;
}

void Cpu::requestInterrupt(int bit)
{
    // Get the value of the interrupt request byte
    // and set the appropriate bit being requested
    // to signify that this is interrupt is reqeusted
    Byte requestedInterrupts = this->mmu->readMemory(INTERRUPT_REQUEST_ADDR);
    setBit(&requestedInterrupts, bit);
    this->mmu->writeMemory(INTERRUPT_REQUEST_ADDR, requestedInterrupts);
}

bool Cpu::isInterruptMaster()
{
    return this->interruptMaster;
}

void Cpu::setInterruptMaster(bool val)
{
    this->interruptMaster = val;
}

void Cpu::serviceInterrupt(int interrupt)
{
    // Disable the master interrupt
    this->setInterruptMaster(false);

    // Unset the interrupt is the request register
    Byte interruptRequest = this->mmu->readMemory(INTERRUPT_REQUEST_ADDR);
    resetBit(&interruptRequest, interrupt);
    this->mmu->writeMemory(INTERRUPT_REQUEST_ADDR, interruptRequest);

    // Interrupt routines can be found at the following locations in memory:
    // V-Blank: 0x40 - bit 0
    // LCD: 0x48 - bit 1
    // TIMER: 0x50 - bit 2
    // JOYPAD: 0x60 - but 4
    // So we need to push the program counter onto the stack, and then set it
    // to the location of the appropriate interrupt we are servicing
    this->pushWordTostack(this->programCounter);
    switch (interrupt)
    {
        case 0: this->programCounter = 0x40; break;
        case 1: this->programCounter = 0x48; break;
        case 2: this->programCounter = 0x50; break;
        case 4: this->programCounter = 0x60; break;
    }
}

void Cpu::pushWordTostack(Word word)
{
    // Push to stack in order of endianess (little) so lower byte will be the first
    // one we pop
    Byte hi = word >> 8;
    Byte lo = word & 0xFF;
    this->mmu->writeMemory(--this->stackPointer.reg, hi);
    this->mmu->writeMemory(--this->stackPointer.reg, lo);
}

Word Cpu::popWordFromStack()
{
    Byte lo = this->mmu->readMemory(this->stackPointer.reg++);
    Byte hi = this->mmu->readMemory(this->stackPointer.reg++);
    return (hi << 8) | lo;
}
