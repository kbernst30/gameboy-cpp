#include "cpu.h"
#include "utils.h"

int Cpu::execute() {
    Byte opcode = this->mmu->readMemory(this->programCounter);
    this->programCounter++;
    return this->doOpcode(opcode);
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

int Cpu::doOpcode(Byte opcode)
{
    // This is where we execute the operation we fetched. Each operation
    // returns a number of clock cycles which we need to return to the
    // main emulation loop to properly handle frame, interrupts, graphics, etc
    // Each operation also might take a certain number of bytes (i.e. if we are loading
    // data into a register, the data is in the next byte). Ensure we increase the PC
    // appropriately
    switch (opcode)
    {
        // No-Op
        case 0x00: return 4; // No-Op - 4 cycles

        // 8-Bit Loads (LD nn, n) - Put immediate 8-bit value n into Register pair nn
        case 0x06: this->do8BitLoad(&(this->bc.parts.hi)); return 8; // LD B, n = 8 cycles
        case 0x0E: this->do8BitLoad(&(this->bc.parts.lo)); return 8; // LD C, n = 8 cycles
        case 0x16: this->do8BitLoad(&(this->de.parts.hi)); return 8; // LD D, n = 8 cycles
        case 0x1E: this->do8BitLoad(&(this->de.parts.lo)); return 8; // LD E, n = 8 cycles
        case 0x26: this->do8BitLoad(&(this->hl.parts.hi)); return 8; // LD H, n = 8 cycles
        case 0x2E: this->do8BitLoad(&(this->hl.parts.lo)); return 8; // LD L, n = 8 cycles

        // 8-Bit Loads (LD r1, r2) - Put value from r2 into r1
        case 0x7F: this->af.parts.hi = this->af.parts.hi;                   return 4;  // LD A, A = 4 cycles
        case 0x78: this->af.parts.hi = this->bc.parts.hi;                   return 4;  // LD A, B = 4 cycles
        case 0x79: this->af.parts.hi = this->bc.parts.lo;                   return 4;  // LD A, C = 4 cycles
        case 0x7A: this->af.parts.hi = this->de.parts.hi;                   return 4;  // LD A, D = 4 cycles
        case 0x7B: this->af.parts.hi = this->de.parts.lo;                   return 4;  // LD A, E = 4 cycles
        case 0x7C: this->af.parts.hi = this->hl.parts.hi;                   return 4;  // LD A, H = 4 cycles
        case 0x7D: this->af.parts.hi = this->hl.parts.lo;                   return 4;  // LD A, L = 4 cycles
        case 0x7E: this->af.parts.hi = this->mmu->readMemory(this->hl.reg); return 8;  // LD A, (HL) = 8 cycles
        case 0x40: this->bc.parts.hi = this->bc.parts.hi;                   return 4;  // LD B, B = 4 cycles
        case 0x41: this->bc.parts.hi = this->bc.parts.lo;                   return 4;  // LD B, C = 4 cycles
        case 0x42: this->bc.parts.hi = this->de.parts.hi;                   return 4;  // LD B, D = 4 cycles
        case 0x43: this->bc.parts.hi = this->de.parts.lo;                   return 4;  // LD B, E = 4 cycles
        case 0x44: this->bc.parts.hi = this->hl.parts.hi;                   return 4;  // LD B, H = 4 cycles
        case 0x45: this->bc.parts.hi = this->hl.parts.hi;                   return 4;  // LD B, L = 4 cycles
        case 0x46: this->bc.parts.hi = this->mmu->readMemory(this->hl.reg); return 8;  // LD B, (HL) = 8 cycles
        case 0x48: this->bc.parts.lo = this->bc.parts.hi;                   return 4;  // LD C, B = 4 cycles
        case 0x49: this->bc.parts.lo = this->bc.parts.lo;                   return 4;  // LD C, C = 4 cycles
        case 0x4A: this->bc.parts.lo = this->de.parts.hi;                   return 4;  // LD C, D = 4 cycles
        case 0x4B: this->bc.parts.lo = this->de.parts.lo;                   return 4;  // LD C, E = 4 cycles
        case 0x4C: this->bc.parts.lo = this->hl.parts.hi;                   return 4;  // LD C, H = 4 cycles
        case 0x4D: this->bc.parts.lo = this->hl.parts.hi;                   return 4;  // LD C, L = 4 cycles
        case 0x4E: this->bc.parts.lo = this->mmu->readMemory(this->hl.reg); return 8;  // LD C, (HL) = 8 cycles
        case 0x50: this->de.parts.hi = this->bc.parts.hi;                   return 4;  // LD D, B = 4 cycles
        case 0x51: this->de.parts.hi = this->bc.parts.lo;                   return 4;  // LD D, C = 4 cycles
        case 0x52: this->de.parts.hi = this->de.parts.hi;                   return 4;  // LD D, D = 4 cycles
        case 0x53: this->de.parts.hi = this->de.parts.lo;                   return 4;  // LD D, E = 4 cycles
        case 0x54: this->de.parts.hi = this->hl.parts.hi;                   return 4;  // LD D, H = 4 cycles
        case 0x55: this->de.parts.hi = this->hl.parts.hi;                   return 4;  // LD D, L = 4 cycles
        case 0x56: this->de.parts.hi = this->mmu->readMemory(this->hl.reg); return 8;  // LD D, (HL) = 8 cycles
        case 0x58: this->de.parts.lo = this->bc.parts.hi;                   return 4;  // LD E, B = 4 cycles
        case 0x59: this->de.parts.lo = this->bc.parts.lo;                   return 4;  // LD E, C = 4 cycles
        case 0x5A: this->de.parts.lo = this->de.parts.hi;                   return 4;  // LD E, D = 4 cycles
        case 0x5B: this->de.parts.lo = this->de.parts.lo;                   return 4;  // LD E, E = 4 cycles
        case 0x5C: this->de.parts.lo = this->hl.parts.hi;                   return 4;  // LD E, H = 4 cycles
        case 0x5D: this->de.parts.lo = this->hl.parts.hi;                   return 4;  // LD E, L = 4 cycles
        case 0x5E: this->de.parts.lo = this->mmu->readMemory(this->hl.reg); return 8;  // LD E, (HL) = 8 cycles
        case 0x60: this->hl.parts.hi = this->bc.parts.hi;                   return 4;  // LD H, B = 4 cycles
        case 0x61: this->hl.parts.hi = this->bc.parts.lo;                   return 4;  // LD H, C = 4 cycles
        case 0x62: this->hl.parts.hi = this->de.parts.hi;                   return 4;  // LD H, D = 4 cycles
        case 0x63: this->hl.parts.hi = this->de.parts.lo;                   return 4;  // LD H, E = 4 cycles
        case 0x64: this->hl.parts.hi = this->hl.parts.hi;                   return 4;  // LD H, H = 4 cycles
        case 0x65: this->hl.parts.hi = this->hl.parts.hi;                   return 4;  // LD H, L = 4 cycles
        case 0x66: this->hl.parts.hi = this->mmu->readMemory(this->hl.reg); return 8;  // LD H, (HL) = 8 cycles
        case 0x68: this->hl.parts.lo = this->bc.parts.hi;                   return 4;  // LD L, B = 4 cycles
        case 0x69: this->hl.parts.lo = this->bc.parts.lo;                   return 4;  // LD L, C = 4 cycles
        case 0x6A: this->hl.parts.lo = this->de.parts.hi;                   return 4;  // LD L, D = 4 cycles
        case 0x6B: this->hl.parts.lo = this->de.parts.lo;                   return 4;  // LD L, E = 4 cycles
        case 0x6C: this->hl.parts.lo = this->hl.parts.hi;                   return 4;  // LD L, H = 4 cycles
        case 0x6D: this->hl.parts.lo = this->hl.parts.hi;                   return 4;  // LD L, L = 4 cycles
        case 0x6E: this->hl.parts.lo = this->mmu->readMemory(this->hl.reg); return 8;  // LD L, (HL) = 8 cycles
        case 0x70: this->mmu->writeMemory(this->hl.reg, this->bc.parts.hi); return 8;  // LD (HL), B = 8 cycles
        case 0x71: this->mmu->writeMemory(this->hl.reg, this->bc.parts.lo); return 8;  // LD (HL), C = 8 cycles
        case 0x72: this->mmu->writeMemory(this->hl.reg, this->de.parts.hi); return 8;  // LD (HL), D = 8 cycles
        case 0x73: this->mmu->writeMemory(this->hl.reg, this->de.parts.lo); return 8;  // LD (HL), E = 8 cycles
        case 0x74: this->mmu->writeMemory(this->hl.reg, this->hl.parts.hi); return 8;  // LD (HL), H = 8 cycles
        case 0x75: this->mmu->writeMemory(this->hl.reg, this->hl.parts.lo); return 8;  // LD (HL), L = 8 cycles
        case 0x36: this->do8BitLoadToMemory(this->hl.reg);                  return 12; // LD (HL), n = 12 cycles

        // 8-Bit Load (LD A, n) - Load n into A
        case 0x0A: this->af.parts.hi = this->mmu->readMemory(this->bc.reg);        return 8;  // LD A, (BC) = 8 cycles
        case 0x1A: this->af.parts.hi = this->mmu->readMemory(this->de.reg);        return 8;  // LD A, (DE) = 8 cycles
        case 0xFA: this->af.parts.hi = this->mmu->readMemory(this->getNextWord()); return 16; // LD A, (nn) = 16 cycles
        case 0x3E: this->do8BitLoad(&(this->af.parts.hi));                         return 8;  // LD A, n = 8 cycles

        // 8-Bit Load (LD n, A) - Load A into n
        case 0x47: this->bc.parts.hi = this->af.parts.hi;                          return 4;  // LD B, A = 4 cycles
        case 0x4F: this->bc.parts.lo = this->af.parts.hi;                          return 4;  // LD C, A = 4 cycles;
        case 0x57: this->de.parts.hi = this->af.parts.hi;                          return 4;  // LD D, A = 4 cycles;
        case 0x5F: this->de.parts.lo = this->af.parts.hi;                          return 4;  // LD E, A = 4 cycles;
        case 0x67: this->hl.parts.hi = this->af.parts.hi;                          return 4;  // LD H, A = 4 cycles;
        case 0x6F: this->hl.parts.lo = this->af.parts.hi;                          return 4;  // LD L, A = 4 cycles;
        case 0x02: this->mmu->writeMemory(this->bc.reg, this->af.parts.hi);        return 8;  // LD (BC), A = 8 cycles
        case 0x12: this->mmu->writeMemory(this->de.reg, this->af.parts.hi);        return 8;  // LD (DE), A = 8 cycles
        case 0x77: this->mmu->writeMemory(this->hl.reg, this->af.parts.hi);        return 8;  // LD (HL), A = 8 cycles
        case 0xEA: this->mmu->writeMemory(this->getNextWord(), this->af.parts.hi); return 16; // LD (nn), A = 16 cycles

        // TODO OpCode table specified the next 2 take 2 bytes but I can't figure out why - the second byte would not be used

        // 8-Bit Load (LD A, (C)) - Load value at address 0xFF00 + value in C into A
        case 0xF2: this->af.parts.hi = this->mmu->readMemory(0xFF00 + this->bc.parts.lo); return 8; // LD A, (C) = 8 cycles

        // 8-Bit Load (LD (C), A) - Load A into address 0xFF00 + value in C
        case 0xE2: this->mmu->writeMemory(0xFF00 + this->bc.parts.lo, this->af.parts.hi); return 8; // LD (C), A = 8 cycles

        // 8-Bit Load (LD A, (HL-)) - Load value at address HL into A and decrement HL
        case 0x3A: this->af.parts.hi = this->mmu->readMemory(this->hl.reg); this->hl.reg--; return 8; // LD A, (HL-) = 8 cycles

        // 8-Bit Load (LD (HL-), A) - Load value A into memory at address HL and decrement HL
        case 0x32: this->mmu->writeMemory(this->hl.reg, this->af.parts.hi); this->hl.reg--; return 8; // LD (HL-), A = 8 cycles

        // 8-Bit Load (LD A, (HL+)) - Load value at address HL into A and increment HL
        case 0x2A: this->af.parts.hi = this->mmu->readMemory(this->hl.reg); this->hl.reg++; return 8; // LD A, (HL+) = 8 cycles

        // 8-Bit Load (LD (HL-), A) - Load value A into memory at address HL and increment HL
        case 0x22: this->mmu->writeMemory(this->hl.reg, this->af.parts.hi); this->hl.reg++; return 8; // LD (HL+), A = 8 cycles

        // 8-Bit Load (LD (n), A) - Load A into address 0xFF00 + value n
        case 0xE0: this->mmu->writeMemory(0xFF00 + this->getNextByte(), this->af.parts.hi); return 12; // LD (n), A = 12 cycles

        // 8-Bit Load (LD A, (n)) - Load value at address 0xFF00 + value n into A
        case 0xF0: this->af.parts.hi = this->mmu->readMemory(0xFF00 + this->getNextByte()); return 12; // LD A, (n) = 12 cycles

        // 16 Bit Load (LD n, nn) - Load immediate 16 bit value into n
        case 0x01: this->do16BitLoad(&(this->bc.reg));           return 12; // LD BC, nn - 12 cycles
        case 0x11: this->do16BitLoad(&(this->de.reg));           return 12; // LD BC, nn - 12 cycles
        case 0x21: this->do16BitLoad(&(this->hl.reg));           return 12; // LD BC, nn - 12 cycles
        case 0x31: this->do16BitLoad(&(this->stackPointer.reg)); return 12; // LD BC, nn - 12 cycles

        // 16 Bit Load - (LD SP, HL) - Load HL into the Stack Pointer
        case 0xF9: this->stackPointer = this->hl; return 8; // LD SP, HL - 8 cycles

        // 16 Bit Load - (LD HL SP+n) - Load Stack pointer plus one byte signed immediate value into HL - 12 cycles
        // Reset Z flag, Reset N flag, Set/reset H flag, set or reset C flag
        case 0xF8: {
            SignedByte offset = (SignedByte) this->getNextByte();
            unsigned int value = this->stackPointer.reg + offset;

            resetBit(&(this->af.parts.lo), ZERO_BIT);
            resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

            // If we are overflowing the max for a Word (0xFFFF) then set carry bit, otherwise reset
            if (value > 0xFFFF)
            {
                setBit(&(this->af.parts.lo), CARRY_BIT);
            }
            else
            {
                resetBit(&(this->af.parts.lo), CARRY_BIT);
            }

            // If we are overflowing lower nibble to upper nibble, then set half carry flag, otherwise reset
            if ((this->stackPointer.reg & 0xF) + (offset & 0xF) > 0xF)
            {
                setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            }
            else
            {
                resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            }

            this->hl.reg = (Word) (value & 0xFFFF);
            return 12;
        }

        // 16 Bit Load - (LD (nn), SP) - Put Stack pointer into memory at nn - 20 cycles
        case 0x08: {
            Word address = this->getNextWord();
            this->mmu->writeMemory(address, this->stackPointer.parts.lo);
            this->mmu->writeMemory(address + 1, this->stackPointer.parts.hi);
            return 20;
        }

        // 16 Bit Load - (PUSH nn) - Push register pair onto the stack and decrememnt stack pointer twice
        case 0xF5: this->pushWordTostack(this->af.reg); return 16; // PUSH AF - 16 cycles
        case 0xC5: this->pushWordTostack(this->bc.reg); return 16; // PUSH BC - 16 cycles
        case 0xD5: this->pushWordTostack(this->de.reg); return 16; // PUSH DE - 16 cycles
        case 0xE5: this->pushWordTostack(this->hl.reg); return 16; // PUSH HL - 16 cycles

        // 16 Bit Load - (POP nn) - Pop two bytes off of the stack into register pair nn
        case 0xF1: this->af.reg = this->popWordFromStack(); return 12; // POP AF - 12 cycles
        case 0xC1: this->bc.reg = this->popWordFromStack(); return 12; // POP BC - 12 cycles
        case 0xD1: this->de.reg = this->popWordFromStack(); return 12; // POP DE - 12 cycles
        case 0xE1: this->hl.reg = this->popWordFromStack(); return 12; // POP HL - 12 cycles

        // 8 Bit ALU - (ADD A, n) - Add n to A
        case 0x87: this->do8BitRegisterAdd(&(this->af.parts.hi), this->af.parts.hi);                   return 4; // ADD A, A - 4 cycles
        case 0x80: this->do8BitRegisterAdd(&(this->af.parts.hi), this->bc.parts.hi);                   return 4; // ADD A, B - 4 cycles
        case 0x81: this->do8BitRegisterAdd(&(this->af.parts.hi), this->bc.parts.lo);                   return 4; // ADD A, C - 4 cycles
        case 0x82: this->do8BitRegisterAdd(&(this->af.parts.hi), this->de.parts.hi);                   return 4; // ADD A, D - 4 cycles
        case 0x83: this->do8BitRegisterAdd(&(this->af.parts.hi), this->de.parts.lo);                   return 4; // ADD A, E - 4 cycles
        case 0x84: this->do8BitRegisterAdd(&(this->af.parts.hi), this->hl.parts.hi);                   return 4; // ADD A, H - 4 cycles
        case 0x85: this->do8BitRegisterAdd(&(this->af.parts.hi), this->hl.parts.lo);                   return 4; // ADD A, L - 4 cycles
        case 0x86: this->do8BitRegisterAdd(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg)); return 8; // ADD A, (HL) - 8 cycles
        case 0xC6: this->do8BitRegisterAdd(&(this->af.parts.hi), this->getNextByte());                 return 8; // ADD A, n - 8 cycles

        // 8 Bit ALU - (ADC A, n) - Add n + carry flag to A
        case 0x8F: this->do8BitRegisterAdd(&(this->af.parts.hi), this->af.parts.hi, true);                   return 4; // ADC A, A - 4 cycles
        case 0x88: this->do8BitRegisterAdd(&(this->af.parts.hi), this->bc.parts.hi, true);                   return 4; // ADC A, B - 4 cycles
        case 0x89: this->do8BitRegisterAdd(&(this->af.parts.hi), this->bc.parts.lo, true);                   return 4; // ADC A, C - 4 cycles
        case 0x8A: this->do8BitRegisterAdd(&(this->af.parts.hi), this->de.parts.hi, true);                   return 4; // ADC A, D - 4 cycles
        case 0x8B: this->do8BitRegisterAdd(&(this->af.parts.hi), this->de.parts.lo, true);                   return 4; // ADC A, E - 4 cycles
        case 0x8C: this->do8BitRegisterAdd(&(this->af.parts.hi), this->hl.parts.hi, true);                   return 4; // ADC A, H - 4 cycles
        case 0x8D: this->do8BitRegisterAdd(&(this->af.parts.hi), this->hl.parts.lo, true);                   return 4; // ADC A, L - 4 cycles
        case 0x8E: this->do8BitRegisterAdd(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg), true); return 8; // ADC A, (HL) - 8 cycles
        case 0xCE: this->do8BitRegisterAdd(&(this->af.parts.hi), this->getNextByte(), true);                 return 8; // ADC A, n - 8 cycles

        // 8 Bit ALU - (SUB n) - Subtract n from A
        case 0x97: this->do8BitRegisterSub(&(this->af.parts.hi), this->af.parts.hi);                   return 4; // SUB A - 4 cycles
        case 0x90: this->do8BitRegisterSub(&(this->af.parts.hi), this->bc.parts.hi);                   return 4; // SUB B - 4 cycles
        case 0x91: this->do8BitRegisterSub(&(this->af.parts.hi), this->bc.parts.lo);                   return 4; // SUB C - 4 cycles
        case 0x92: this->do8BitRegisterSub(&(this->af.parts.hi), this->de.parts.hi);                   return 4; // SUB D - 4 cycles
        case 0x93: this->do8BitRegisterSub(&(this->af.parts.hi), this->de.parts.lo);                   return 4; // SUB E - 4 cycles
        case 0x94: this->do8BitRegisterSub(&(this->af.parts.hi), this->hl.parts.hi);                   return 4; // SUB H - 4 cycles
        case 0x95: this->do8BitRegisterSub(&(this->af.parts.hi), this->hl.parts.lo);                   return 4; // SUB L - 4 cycles
        case 0x96: this->do8BitRegisterSub(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg)); return 8; // SUB (HL) - 8 cycles
        case 0xD6: this->do8BitRegisterSub(&(this->af.parts.hi), this->getNextByte());                 return 8; // SUB n - 8 cycles

        // 8 Bit ALU - (SBC A, n) - Subtract n + carry flag from A
        case 0x9F: this->do8BitRegisterSub(&(this->af.parts.hi), this->af.parts.hi, true);                   return 4; // SBC A, A - 4 cycles
        case 0x98: this->do8BitRegisterSub(&(this->af.parts.hi), this->bc.parts.hi, true);                   return 4; // SBC A, B - 4 cycles
        case 0x99: this->do8BitRegisterSub(&(this->af.parts.hi), this->bc.parts.lo, true);                   return 4; // SBC A, C - 4 cycles
        case 0x9A: this->do8BitRegisterSub(&(this->af.parts.hi), this->de.parts.hi, true);                   return 4; // SBC A, D - 4 cycles
        case 0x9B: this->do8BitRegisterSub(&(this->af.parts.hi), this->de.parts.lo, true);                   return 4; // SBC A, E - 4 cycles
        case 0x9C: this->do8BitRegisterSub(&(this->af.parts.hi), this->hl.parts.hi, true);                   return 4; // SBC A, H - 4 cycles
        case 0x9D: this->do8BitRegisterSub(&(this->af.parts.hi), this->hl.parts.lo, true);                   return 4; // SBC A, L - 4 cycles
        case 0x9E: this->do8BitRegisterSub(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg), true); return 8; // SBC A, (HL) - 8 cycles
        case 0xDE: this->do8BitRegisterSub(&(this->af.parts.hi), this->getNextByte(), true);                 return 8; // SBC A, n - 8 cycles

        // 8 Bit ALU - (AND n) - AND n with A
        case 0xA7: this->do8BitRegisterAnd(&(this->af.parts.hi), this->af.parts.hi);                   return 4; // AND A - 4 cycles
        case 0xA0: this->do8BitRegisterAnd(&(this->af.parts.hi), this->bc.parts.hi);                   return 4; // AND B - 4 cycles
        case 0xA1: this->do8BitRegisterAnd(&(this->af.parts.hi), this->bc.parts.lo);                   return 4; // AND C - 4 cycles
        case 0xA2: this->do8BitRegisterAnd(&(this->af.parts.hi), this->de.parts.hi);                   return 4; // AND D - 4 cycles
        case 0xA3: this->do8BitRegisterAnd(&(this->af.parts.hi), this->de.parts.lo);                   return 4; // AND E - 4 cycles
        case 0xA4: this->do8BitRegisterAnd(&(this->af.parts.hi), this->hl.parts.hi);                   return 4; // AND H - 4 cycles
        case 0xA5: this->do8BitRegisterAnd(&(this->af.parts.hi), this->hl.parts.lo);                   return 4; // AND L - 4 cycles
        case 0xA6: this->do8BitRegisterAnd(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg)); return 8; // AND (HL) - 8 cycles
        case 0xE6: this->do8BitRegisterAnd(&(this->af.parts.hi), this->getNextByte());                 return 8; // AND n - 8 cycles

        // 8 Bit ALU - (OR n) - OR n with A
        case 0xB7: this->do8BitRegisterAnd(&(this->af.parts.hi), this->af.parts.hi);                   return 4; // OR A - 4 cycles
        case 0xB0: this->do8BitRegisterAnd(&(this->af.parts.hi), this->bc.parts.hi);                   return 4; // OR B - 4 cycles
        case 0xB1: this->do8BitRegisterAnd(&(this->af.parts.hi), this->bc.parts.lo);                   return 4; // OR C - 4 cycles
        case 0xB2: this->do8BitRegisterAnd(&(this->af.parts.hi), this->de.parts.hi);                   return 4; // OR D - 4 cycles
        case 0xB3: this->do8BitRegisterAnd(&(this->af.parts.hi), this->de.parts.lo);                   return 4; // OR E - 4 cycles
        case 0xB4: this->do8BitRegisterAnd(&(this->af.parts.hi), this->hl.parts.hi);                   return 4; // OR H - 4 cycles
        case 0xB5: this->do8BitRegisterAnd(&(this->af.parts.hi), this->hl.parts.lo);                   return 4; // OR L - 4 cycles
        case 0xB6: this->do8BitRegisterAnd(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg)); return 8; // OR (HL) - 8 cycles
        case 0xF6: this->do8BitRegisterAnd(&(this->af.parts.hi), this->getNextByte());                 return 8; // OR n - 8 cycles

        // 8 Bit ALU - (XOR n) - XOR n with A
        case 0xAF: this->do8BitRegisterXor(&(this->af.parts.hi), this->af.parts.hi);                   return 4; // XOR A - 4 cycles
        case 0xA8: this->do8BitRegisterXor(&(this->af.parts.hi), this->bc.parts.hi);                   return 4; // XOR B - 4 cycles
        case 0xA9: this->do8BitRegisterXor(&(this->af.parts.hi), this->bc.parts.lo);                   return 4; // XOR C - 4 cycles
        case 0xAA: this->do8BitRegisterXor(&(this->af.parts.hi), this->de.parts.hi);                   return 4; // XOR D - 4 cycles
        case 0xAB: this->do8BitRegisterXor(&(this->af.parts.hi), this->de.parts.lo);                   return 4; // XOR E - 4 cycles
        case 0xAC: this->do8BitRegisterXor(&(this->af.parts.hi), this->hl.parts.hi);                   return 4; // XOR H - 4 cycles
        case 0xAD: this->do8BitRegisterXor(&(this->af.parts.hi), this->hl.parts.lo);                   return 4; // XOR L - 4 cycles
        case 0xAE: this->do8BitRegisterXor(&(this->af.parts.hi), this->mmu->readMemory(this->hl.reg)); return 8; // XOR (HL) - 8 cycles
        case 0xEE: this->do8BitRegisterXor(&(this->af.parts.hi), this->getNextByte());                 return 8; // XOR n - 8 cycles

        // 8 Bit ALU - (CP n) - Compare A with n - basically a subtract where we throw away value
        case 0xBF: this->do8BitRegisterCompare(this->af.parts.hi, this->af.parts.hi);                   return 4; // CP A - 4 cycles
        case 0xB8: this->do8BitRegisterCompare(this->af.parts.hi, this->bc.parts.hi);                   return 4; // CP B - 4 cycles
        case 0xB9: this->do8BitRegisterCompare(this->af.parts.hi, this->bc.parts.lo);                   return 4; // CP C - 4 cycles
        case 0xBA: this->do8BitRegisterCompare(this->af.parts.hi, this->de.parts.hi);                   return 4; // CP D - 4 cycles
        case 0xBB: this->do8BitRegisterCompare(this->af.parts.hi, this->de.parts.lo);                   return 4; // CP E - 4 cycles
        case 0xBC: this->do8BitRegisterCompare(this->af.parts.hi, this->hl.parts.hi);                   return 4; // CP H - 4 cycles
        case 0xBD: this->do8BitRegisterCompare(this->af.parts.hi, this->hl.parts.lo);                   return 4; // CP L - 4 cycles
        case 0xBE: this->do8BitRegisterCompare(this->af.parts.hi, this->mmu->readMemory(this->hl.reg)); return 8; // CP (HL) - 8 cycles
        case 0xFE: this->do8BitRegisterCompare(this->af.parts.hi, this->getNextByte());                 return 8; // CP n - 8 cycles

        // 8 Bit ALU - (INC n) - Increment register n
        case 0x3C: this->do8BitRegisterIncrement(&(this->af.parts.hi)); return 4; // INC A - 4 cycles
        case 0x04: this->do8BitRegisterIncrement(&(this->bc.parts.hi)); return 4; // INC B - 4 cycles
        case 0x0C: this->do8BitRegisterIncrement(&(this->bc.parts.lo)); return 4; // INC C - 4 cycles
        case 0x14: this->do8BitRegisterIncrement(&(this->de.parts.hi)); return 4; // INC D - 4 cycles
        case 0x1C: this->do8BitRegisterIncrement(&(this->de.parts.lo)); return 4; // INC E - 4 cycles
        case 0x24: this->do8BitRegisterIncrement(&(this->hl.parts.hi)); return 4; // INC H - 4 cycles
        case 0x2C: this->do8BitRegisterIncrement(&(this->hl.parts.lo)); return 4; // INC L - 4 cycles
        // INC (HL) - 12 cycles
        case 0x34: {
            Byte temp = this->mmu->readMemory(this->hl.reg);;
            this->do8BitRegisterIncrement(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 12;
        }

        // 8 Bit ALU - (DEC n) - Decrement register n
        case 0x3D: this->do8BitRegisterDecrement(&(this->af.parts.hi)); return 4; // DEC A - 4 cycles
        case 0x05: this->do8BitRegisterDecrement(&(this->bc.parts.hi)); return 4; // DEC B - 4 cycles
        case 0x0D: this->do8BitRegisterDecrement(&(this->bc.parts.lo)); return 4; // DEC C - 4 cycles
        case 0x15: this->do8BitRegisterDecrement(&(this->de.parts.hi)); return 4; // DEC D - 4 cycles
        case 0x1D: this->do8BitRegisterDecrement(&(this->de.parts.lo)); return 4; // DEC E - 4 cycles
        case 0x25: this->do8BitRegisterDecrement(&(this->hl.parts.hi)); return 4; // DEC H - 4 cycles
        case 0x2D: this->do8BitRegisterDecrement(&(this->hl.parts.lo)); return 4; // DEC L - 4 cycles
        // DEC (HL) - 12 cycles
        case 0x35: {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterDecrement(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 12;
        }

        // 16 Bit Arithmetic - (ADD HL, n) - Add n to HL
        case 0x09: this->do16BitRegisterAdd(&(this->hl.reg), this->bc.reg);           return 8; // ADD HL, BC - 8 cycles
        case 0x19: this->do16BitRegisterAdd(&(this->hl.reg), this->de.reg);           return 8; // ADD HL, DE - 8 cycles
        case 0x29: this->do16BitRegisterAdd(&(this->hl.reg), this->hl.reg);           return 8; // ADD HL, HL - 8 cycles
        case 0x39: this->do16BitRegisterAdd(&(this->hl.reg), this->stackPointer.reg); return 8; // ADD HL, SP - 8 cycles

        // 16 Bit Arithmetic - (ADD SP, n) - Add n to SP - 16 cycles
        // Reset Z flag, Reset N flag, Set/reset H flag, set or reset C flag
        case 0xE8: {
            SignedByte offset = (SignedByte) this->getNextByte();
            unsigned int value = this->stackPointer.reg + offset;

            resetBit(&(this->af.parts.lo), ZERO_BIT);
            resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

            // If we are overflowing the max for a Word (0xFFFF) then set carry bit, otherwise reset
            if (value > 0xFFFF)
            {
                setBit(&(this->af.parts.lo), CARRY_BIT);
            }
            else
            {
                resetBit(&(this->af.parts.lo), CARRY_BIT);
            }

            // If we are overflowing lower nibble to upper nibble, then set half carry flag, otherwise reset
            if ((this->stackPointer.reg & 0xF) + (value & 0xF) > 0xF)
            {
                setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            }
            else
            {
                resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            }

            this->stackPointer.reg = (Word) (value & 0xFFFF);
            return 16;
        }

        // 16 Bit Arithmethc - (INC nn) - Increment register pair nn
        case 0x03: this->bc.reg++;           return 8; // INC BC = 8 cycles
        case 0x13: this->de.reg++;           return 8; // INC DE = 8 cycles
        case 0x23: this->hl.reg++;           return 8; // INC HL = 8 cycles
        case 0x33: this->stackPointer.reg++; return 8; // INC SP = 8 cycles

        // 16 Bit Arithmethc - (DEC nn) - Decrement register pair nn
        case 0x0B: this->bc.reg--;           return 8; // DEC BC = 8 cycles
        case 0x1B: this->de.reg--;           return 8; // DEC DE = 8 cycles
        case 0x2B: this->hl.reg--;           return 8; // DEC HL = 8 cycles
        case 0x3B: this->stackPointer.reg--; return 8; // DEC SP = 8 cycles
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

Word Cpu::getNextWord()
{
    Byte data1 = this->mmu->readMemory(this->programCounter);
    this->programCounter++;
    Byte data2 = this->mmu->readMemory(this->programCounter);
    this->programCounter++;

    return (data2 << 8) | data1;
}

Byte Cpu::getNextByte()
{
    Byte data = this->mmu->readMemory(this->programCounter);
    this->programCounter++;
    return data;
}

void Cpu::do8BitLoad(Byte *reg)
{
    *reg = getNextByte();
}

void Cpu::do8BitLoadToMemory(Word address)
{
    this->mmu->writeMemory(address, getNextByte());
}

void Cpu::do16BitLoad(Word *reg)
{
    *reg = getNextWord();
}

void Cpu::do8BitRegisterAdd(Byte *reg, Byte value, bool useCarry)
{
    // This should perform an 8 bit add operation and store
    // the result in register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be reset
    // The Half-Carry flag should be set if we carry from bit 3
    // The Carry flag should be set if we carry from but 7

    int result = *reg + value;
    if (useCarry && isBitSet(this->af.parts.lo, CARRY_BIT))
    {
        result++;
    }

    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if ((Byte) (result & 0xFF) == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    if (result > 0xFF)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }

    Word lowerNibble = *reg & 0xF;
    if (lowerNibble + (value & 0xF) > 0xF)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }

    *reg = (Byte) (result & 0xFF);
}

void Cpu::do16BitRegisterAdd(Word *reg, Byte value)
{
    // This should perform a 16 bit add operation and store
    // the result in register reg
    // The Zero flag is not affected
    // THe Subtract flag should be reset
    // The Half-Carry flag should be set if we carry from bit 11
    // The Carry flag should be set if we carry from but 15

    int result = *reg + value;
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (result > 0xFFFF)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }

    Word lowerTwelve = *reg & 0xFFF;
    if (lowerTwelve + (value & 0xFFF) > 0xFFF)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }

    *reg = (Word) (result & 0xFFFF);
}

void Cpu::do8BitRegisterSub(Byte *reg, Byte value, bool useCarry)
{
    // This should perform an 8 bit subtract operation and store
    // the result in register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be set
    // The Half-Carry flag should be set if we do not borrow from bit 4
    // The Carry flag should be set if we do not borrow

    Byte toSubtract = value;
    if (useCarry && isBitSet(this->af.parts.lo, CARRY_BIT))
    {
        toSubtract--;
    }

    setBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (*reg - toSubtract == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    if (*reg < toSubtract)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }

    Word lowerNibble = *reg & 0xF;
    if ((toSubtract & 0xF) < lowerNibble)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }

    *reg = (Byte) ((*reg - toSubtract) & 0xFF);
}

void Cpu::do8BitRegisterAnd(Byte *reg, Byte value)
{
    // This should perform an 8 bit logical AND operation and store
    // the result in register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be reset
    // The Half-Carry flag should be set
    // The Carry flag should be reset

    *reg &= value;
    if (*reg == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
    resetBit(&(this->af.parts.lo), CARRY_BIT);
}

void Cpu::do8BitRegisterOr(Byte *reg, Byte value)
{
    // This should perform an 8 bit logical OR operation and store
    // the result in register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be reset
    // The Half-Carry flag should be reset
    // The Carry flag should be reset

    *reg |= value;
    if (*reg == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
    resetBit(&(this->af.parts.lo), CARRY_BIT);
}

void Cpu::do8BitRegisterXor(Byte *reg, Byte value)
{
    // This should perform an 8 bit logical XOR operation and store
    // the result in register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be reset
    // The Half-Carry flag should be reset
    // The Carry flag should be reset

    *reg ^= value;
    if (*reg == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
    resetBit(&(this->af.parts.lo), CARRY_BIT);
}

void Cpu::do8BitRegisterCompare(Byte source, Byte value)
{
    // This should perform an 8 bit compare operation
    // The Zero flag should be set if the comparison is zero (source == value)
    // THe Subtract flag should be set
    // The Half-Carry flag should be set if we do not borrow from bit 4
    // The Carry flag should be set if we do not borrow (source < value)

    setBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (source == value)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    if (source < value)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }

    Word lowerNibble = source & 0xF;
    if ((value & 0xF) < lowerNibble)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
}

void Cpu::do8BitRegisterIncrement(Byte *reg)
{
    // This should perform an 8 bit increment of register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be reset
    // The Half-Carry flag should be set if we carry from bit 3
    // The Carry flag is not affected

    int result = *reg + 1;

    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if ((Byte) (result & 0xFF) == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    Word lowerNibble = *reg & 0xF;
    if (lowerNibble + 1 > 0xF)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }

    *reg = (Byte) (result & 0xFF);
}

void Cpu::do8BitRegisterDecrement(Byte *reg)
{
    // This should perform an 8 bit decrement of register reg
    // The Zero flag should be set if the result is zero
    // THe Subtract flag should be set
    // The Half-Carry flag should be set if we do not borrow from bit 4
    // The Carry flag is not affected

    Byte result = *reg - 1;

    setBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (result == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }

    Word lowerNibble = *reg & 0xF;
    if (1 < lowerNibble)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }

    *reg = (Byte) (result & 0xFF);
}