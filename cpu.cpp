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

    this->progamCounter = 0x100;
    this->stackPointer.reg = 0xFFFE;
}
