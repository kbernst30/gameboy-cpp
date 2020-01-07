#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <memory>
#include <iostream>
#include <fstream>

#include "cpu.h"
#include "utils.h"

void Cpu::debug()
{
    printf("A: 0x%.2x B: 0x%.2x C: 0x%.2x D: 0x%.2x\n E: 0x%.2x F: 0x%.2x: H: 0x%.2x L: 0x%.2x\n", this->af.parts.hi, this->bc.parts.hi, this->bc.parts.lo, this->de.parts.hi, this->de.parts.lo, this->af.parts.lo, this->hl.parts.hi, this->hl.parts.lo);
    printf("PC: 0x%.4x\n", this->programCounter);
}

int Cpu::execute()
{
    int cycles;

    // printf("A: 0x%.2x B: 0x%.2x C: 0x%.2x D: 0x%.2x\n E: 0x%.2x F: 0x%.2x: H: 0x%.2x L: 0x%.2x\n", this->af.parts.hi, this->bc.parts.hi, this->bc.parts.lo, this->de.parts.hi, this->de.parts.lo, this->af.parts.lo, this->hl.parts.hi, this->hl.parts.lo);

    if (!this->halted)
    {
        Byte opcode = this->mmu->readMemory(this->programCounter);
        // printf("OPCODE: 0x%.2x PC: 0x%.4x\n", opcode, this->programCounter);

        this->programCounter++;
        cycles = this->doOpcode(opcode);
    }
    else
    {
        cycles = 4;
    }

    // We need to see based on pending flags if we should enable/disable
    // interrupts. This should only happen if the instruction before the
    // last one executed was DI (0xF3) or EI (0xFB)
    Byte lastOpcode = this->mmu->readMemory(this->programCounter - 2);
    if (lastOpcode == 0xF3 && this->willDisableInterrupts)
    {
        this->willDisableInterrupts = false;
        this->interruptMaster = false;
    }
    else if (lastOpcode == 0xFB && this->willEnableInterrupts)
    {
        this->willEnableInterrupts = false;
        this->interruptMaster = true;
    }

    return cycles;
}

void Cpu::reset()
{
    // This is the initial state of the CPU registers
    // program counter, and stack pointer. The following
    // is documented in Gameboy architecture
    this->af.reg = 0x01B0;
    this->bc.reg = 0x0013;
    this->de.reg = 0x00D8;
    this->hl.reg = 0x014D;

    this->programCounter = 0x100;
    this->stackPointer.reg = 0xFFFE;

    this->interruptMaster = true;
    this->willDisableInterrupts = false;
    this->willEnableInterrupts = false;

    this->halted = false;
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
    // Unhalt the CPU
    this->halted = false;

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
        // CB Table - This is where we need to execute extended opcode from secondary CB table
        case 0xCB: return this->doExtendedOpcode(this->getNextByte());

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

        // TODO OpCode table specified the next 2 take 2 bytes but I can't figure out why - the second byte would not be used - add second byte for now

        // 8-Bit Load (LD A, (C)) - Load value at address 0xFF00 + value in C into A
        case 0xF2: this->af.parts.hi = this->mmu->readMemory(0xFF00 + this->bc.parts.lo); this->programCounter += 1; return 8; // LD A, (C) = 8 cycles

        // 8-Bit Load (LD (C), A) - Load A into address 0xFF00 + value in C
        case 0xE2: this->mmu->writeMemory(0xFF00 + this->bc.parts.lo, this->af.parts.hi); this->programCounter += 1; return 8; // LD (C), A = 8 cycles

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
        case 0xF8:
        {
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
        case 0x08:
        {
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
        case 0x34:
        {
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
        case 0x35:
        {
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
        case 0xE8:
        {
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

        // Misc - (DAA) - Decimal Adjust Register A - 4 cycles
        // Set Z flag if register A is zero, Reset H flag, set/reset C flag, N flag not affected
        case 0x27:
        {
            // This should adjust the value in register A so that it proper BCD represetnation
            // where the value SHOULD be the result of a previous ADD or SUB of two BCD numbers
            // To adjust properly, we need to add or subtract from the current value
            // TODO I really need to understand this better
            Byte upper = this->af.parts.hi >> 4;
            Byte lower = this->af.parts.hi & 0xF;

            if (isBitSet(this->af.parts.lo, SUBTRACT_BIT))
            {
                // Previous operation was a subtract
                // If half carry, we need to subtract 6 from the lower nibble
                // If carry, we need to subtract 6 from the upper nibble
                if (isBitSet(this->af.parts.lo, HALF_CARRY_BIT)) lower -= 0x6;
                if (isBitSet(this->af.parts.lo, CARRY_BIT)) upper -= 6;
            }
            else
            {
                // Previous operation was an add
                // If half carry or first nibble is greater than 9, we need to add 6 to lower nibble
                // If carry or second nibble is greater than 9, we need to add 6 from the upper nibble
                if (isBitSet(this->af.parts.lo, HALF_CARRY_BIT) || lower > 9) lower += 0x6;
                if (isBitSet(this->af.parts.lo, CARRY_BIT) || upper > 9) upper += 6;
            }

            Byte bcd = (upper << 4) | lower;

            if (bcd == 0)
            {
                setBit(&(this->af.parts.lo), ZERO_BIT);
            }

            resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);

            this->af.parts.hi = bcd;
            return 4;
        }

        // Misc - (CPL) - Complement Register A - 4 cycles
        // Set N flag and Set H flag
        case 0x2F:
        {
            this->af.parts.hi ^= 0xFF;
            setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            setBit(&(this->af.parts.lo), SUBTRACT_BIT);
            return 4;
        }

        // Misc - (CCF) - Complement Carry Flag - 4 cycles
        // Reset N flag and reset H flag
        case 0x3F:
        {
            if (isBitSet(this->af.parts.lo, CARRY_BIT)) resetBit(&(this->af.parts.lo), CARRY_BIT);
            else setBit(&(this->af.parts.lo), CARRY_BIT);
            resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
            return 4;
        }

        // Misc - (SCF) - Set Carry Flag - 4 cycles
        // Reset N flag and reset H flag
        case 0x37:
        {
            setBit(&(this->af.parts.lo), CARRY_BIT);
            resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
            resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
            return 4;
        }

        // Misc - (HALT) - Powers down CPU until interrupt occurs
        case 0x76: this->halted = true; return 4; // HALT - 4 cycles

        // Misc - (STOP) - Halt CPU and LCD until button pressed
        case 0x10: return 4; // STOP - 4 cycles - // TODO should I halt here or use different flag?

        // Misc - (DI) - Disable interrupts after the NEXT instruction
        case 0xF3: this->willDisableInterrupts = true; return 4; // DI - 4 cycles

        // Misc - (EI) - Enable interrupts after the NEXT instruction
        case 0xFB: this->willEnableInterrupts = true; return 4; // DI - 4 cycles

        // Rotate - (RLCA) - Rotate A left, Bit 7 to Carry flag
        case 0x07: this->do8BitRegisterRotateLeft(&(this->af.parts.hi)); return 4; // RLCA - 4 cycles

        // Rotate - (RLA) - Rotate A left, through Carry flag
        case 0x17: this->do8BitRegisterRotateLeft(&(this->af.parts.hi), true); return 4; // RLA - 4 cycles

        // Rotate - (RRCA) - Rotate A right, Bit 7 to Carry flag
        case 0x0F: this->do8BitRegisterRotateRight(&(this->af.parts.hi)); return 4; // RRCA - 4 cycles

        // Rotate - (RRA) - Rotate A right, through Carry flag
        case 0x1F: this->do8BitRegisterRotateRight(&(this->af.parts.hi), true); return 4; // RRA - 4 cycles

        // Jump - (JP nn) - Jump to address nn, immediate two byte value
        case 0xC3: this->programCounter = this->getNextWord(); return 12; // JP nn - 12 cycles

        // Jump - (JP cc, nn) - Jump to address nn, immediate two byte value, if cc is true
        // cc = NZ => Z flag is reset
        // cc = Z => Z flag is set
        // cc = NC => C flag is reset
        // cc = C => C flag is set
        case 0xC2: this->programCounter = !isBitSet(this->af.parts.lo, ZERO_BIT) ? this->getNextWord() : this->programCounter + 2;  return 12; // JP NZ, nn - 12 cycles
        case 0xCA: this->programCounter = isBitSet(this->af.parts.lo, ZERO_BIT) ? this->getNextWord() : this->programCounter + 2;   return 12; // JP Z, nn - 12 cycles
        case 0xD2: this->programCounter = !isBitSet(this->af.parts.lo, CARRY_BIT) ? this->getNextWord() : this->programCounter + 2; return 12; // JP NC, nn - 12 cycles
        case 0xDA: this->programCounter = isBitSet(this->af.parts.lo, CARRY_BIT) ? this->getNextWord() : this->programCounter + 2;  return 12; // JP C, nn - 12 cycles

        // Jump - (JP (HL)) - Jump to address contained in HL
        case 0xE9: this->programCounter = this->mmu->readMemory(this->hl.reg); return 4; // JP (HL) - 4 cycles

        // Jump - (JR n) - Add n to current address and jump, n is signed
        case 0x18: this->programCounter = this->programCounter + 1 + ((SignedByte) this->getNextByte()); return 8; // JR n - 8 cycles

        // Jump - (JR cc, n) - Add n to current address and jump, n is signed, if cc is true
        // cc = NZ => Z flag is reset
        // cc = Z => Z flag is set
        // cc = NC => C flag is reset
        // cc = C => C flag is set
        case 0x20: this->programCounter = !isBitSet(this->af.parts.lo, ZERO_BIT) ? this->programCounter + 1 + ((SignedByte) this->getNextByte()) : this->programCounter + 1;  return 8; // JR NZ, n - 8 cycles
        case 0x28: this->programCounter = isBitSet(this->af.parts.lo, ZERO_BIT) ? this->programCounter + 1 + ((SignedByte) this->getNextByte()) : this->programCounter + 1;   return 8; // JR Z, n - 8 cycles
        case 0x30: this->programCounter = !isBitSet(this->af.parts.lo, CARRY_BIT) ? this->programCounter + 1 + ((SignedByte) this->getNextByte()) : this->programCounter + 1; return 8; // JR NC, n - 8 cycles
        case 0x38: this->programCounter = isBitSet(this->af.parts.lo, CARRY_BIT) ? this->programCounter + 1 + ((SignedByte) this->getNextByte()) : this->programCounter + 1;  return 8; // JR C, n - 8 cycles

        // Call - (CALL nn) - Push address of next instruction (current PC + 2 as inst takes 3 bytes) onto stack and then jump to address nn
        case 0xCD: this->pushWordTostack(this->programCounter + 2); this->programCounter = this->getNextWord(); return 12; // CALL nnn - 12 cycles

        // Call - (CALL cc, nn) - Call address nn, immediate two byte value, if cc is true
        // cc = NZ => Z flag is reset
        // cc = Z => Z flag is set
        // cc = NC => C flag is reset
        // cc = C => C flag is set
        // CALL NZ, nn - 12 cycles
        case 0xC4:
        {
            if (!isBitSet(this->af.parts.lo, ZERO_BIT))
            {
                this->pushWordTostack(this->programCounter + 2);
                this->programCounter = this->getNextWord();
            }
            else
            {
                this->programCounter += 2;
            }

            return 12;
        }
        // CALL Z, nn - 12 cycles
        case 0xCC:
        {
            if (isBitSet(this->af.parts.lo, ZERO_BIT))
            {
                this->pushWordTostack(this->programCounter + 2);
                this->programCounter = this->getNextWord();
            }
            else
            {
                this->programCounter += 2;
            }

            return 12;
        }
        // CALL NC, nn - 12 cycles
        case 0xD4:
        {
            if (!isBitSet(this->af.parts.lo, CARRY_BIT))
            {
                this->pushWordTostack(this->programCounter + 2);
                this->programCounter = this->getNextWord();
            }
            else
            {
                this->programCounter += 2;
            }

            return 12;
        }
        // CALL C, nn - 12 cycles
        case 0xDC:
        {
            if (isBitSet(this->af.parts.lo, CARRY_BIT))
            {
                this->pushWordTostack(this->programCounter + 2);
                this->programCounter = this->getNextWord();
            }
            else
            {
                this->programCounter += 2;
            }

            return 12;
        }

        // Restart - (RST n) - Push present address onto stack, jump to $0000 + n
        case 0xC7: this->pushWordTostack(this->programCounter); this->programCounter = 0x00; return 32; // RST 00 - 32 cycles
        case 0xCF: this->pushWordTostack(this->programCounter); this->programCounter = 0x08; return 32; // RST 08 - 32 cycles
        case 0xD7: this->pushWordTostack(this->programCounter); this->programCounter = 0x10; return 32; // RST 10 - 32 cycles
        case 0xDF: this->pushWordTostack(this->programCounter); this->programCounter = 0x18; return 32; // RST 18 - 32 cycles
        case 0xE7: this->pushWordTostack(this->programCounter); this->programCounter = 0x20; return 32; // RST 20 - 32 cycles
        case 0xEF: this->pushWordTostack(this->programCounter); this->programCounter = 0x28; return 32; // RST 28 - 32 cycles
        case 0xF7: this->pushWordTostack(this->programCounter); this->programCounter = 0x30; return 32; // RST 30 - 32 cycles
        case 0xFF: this->pushWordTostack(this->programCounter); this->programCounter = 0x38; return 32; // RST 38 - 32 cycles

        // Return - (RET) - Pop two bytes from stack and jump to that address
        case 0xC9: this->programCounter = this->popWordFromStack(); return 8; // RET - 8 cycles

        // Return - (RET cc) - Return if cc is true
        // cc = NZ => Z flag is reset
        // cc = Z => Z flag is set
        // cc = NC => C flag is reset
        // cc = C => C flag is set
        case 0xC0: this->programCounter = !isBitSet(this->af.parts.lo, ZERO_BIT) ? this->popWordFromStack() : this->programCounter;  return 8; // RET NZ - 8 cycles
        case 0xC8: this->programCounter = isBitSet(this->af.parts.lo, ZERO_BIT) ? this->popWordFromStack() : this->programCounter;   return 8; // RET Z - 8 cycles
        case 0xD0: this->programCounter = !isBitSet(this->af.parts.lo, CARRY_BIT) ? this->popWordFromStack() : this->programCounter; return 8; // RET NC - 8 cycles
        case 0xD8: this->programCounter = isBitSet(this->af.parts.lo, CARRY_BIT) ? this->popWordFromStack() : this->programCounter;  return 8; // RET C - 8 cycles

        // Return - (RETI) - Pop two bytes from stack and jump to that address, then enable interrupts
        case 0xD9: this->programCounter = this->popWordFromStack(); this->interruptMaster = true; return 8; // RETI - 8 cycles

        default:
            printf("unknown op: 0x%.2x\n", opcode);
            printf("PC was at 0x%.4x\n", this->programCounter);
            return 4;
    }
}

int Cpu::doExtendedOpcode(Byte opcode)
{
    // Opcode CB results in a lookup in a secondary opcode table. This is where we will check
    // those operations
    switch (opcode)
    {
        // Misc - (SWAP n) - Swap upper and lower nibbles of n
        case 0x37: this->do8BitRegisterSwap(&(this->af.parts.hi)); return 8; // SWAP A - 8 cycles
        case 0x30: this->do8BitRegisterSwap(&(this->bc.parts.hi)); return 8; // SWAP B - 8 cycles
        case 0x31: this->do8BitRegisterSwap(&(this->bc.parts.lo)); return 8; // SWAP C - 8 cycles
        case 0x32: this->do8BitRegisterSwap(&(this->de.parts.hi)); return 8; // SWAP D - 8 cycles
        case 0x33: this->do8BitRegisterSwap(&(this->de.parts.lo)); return 8; // SWAP E - 8 cycles
        case 0x34: this->do8BitRegisterSwap(&(this->hl.parts.hi)); return 8; // SWAP H - 8 cycles
        case 0x35: this->do8BitRegisterSwap(&(this->hl.parts.lo)); return 8; // SWAP L - 8 cycles
        // INC (HL) - 12 cycles
        case 0x36:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterSwap(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Rotate - (RLC n) - Rotate n left, Bit 7 to Carry flag
        case 0x07: this->do8BitRegisterRotateLeft(&(this->af.parts.hi)); return 8; // RLC A - 8 cycles
        case 0x00: this->do8BitRegisterRotateLeft(&(this->bc.parts.hi)); return 8; // RLC B - 8 cycles
        case 0x01: this->do8BitRegisterRotateLeft(&(this->bc.parts.lo)); return 8; // RLC C - 8 cycles
        case 0x02: this->do8BitRegisterRotateLeft(&(this->de.parts.hi)); return 8; // RLC D - 8 cycles
        case 0x03: this->do8BitRegisterRotateLeft(&(this->de.parts.lo)); return 8; // RLC E - 8 cycles
        case 0x04: this->do8BitRegisterRotateLeft(&(this->hl.parts.hi)); return 8; // RLC H - 8 cycles
        case 0x05: this->do8BitRegisterRotateLeft(&(this->hl.parts.lo)); return 8; // RLC L - 8 cycles
        // RLC (HL) - 16 cycles
        case 0x06:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterRotateLeft(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Rotate - (RL n) - Rotate n left through carry flag
        case 0x17: this->do8BitRegisterRotateLeft(&(this->af.parts.hi), true); return 8; // RL A - 8 cycles
        case 0x10: this->do8BitRegisterRotateLeft(&(this->bc.parts.hi), true); return 8; // RL B - 8 cycles
        case 0x11: this->do8BitRegisterRotateLeft(&(this->bc.parts.lo), true); return 8; // RL C - 8 cycles
        case 0x12: this->do8BitRegisterRotateLeft(&(this->de.parts.hi), true); return 8; // RL D - 8 cycles
        case 0x13: this->do8BitRegisterRotateLeft(&(this->de.parts.lo), true); return 8; // RL E - 8 cycles
        case 0x14: this->do8BitRegisterRotateLeft(&(this->hl.parts.hi), true); return 8; // RL H - 8 cycles
        case 0x15: this->do8BitRegisterRotateLeft(&(this->hl.parts.lo), true); return 8; // RL L - 8 cycles
        // RL (HL) - 16 cycles
        case 0x16:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterRotateLeft(&temp, true);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Rotate - (RRC n) - Rotate n right, Bit 0 to Carry flag
        case 0x0F: this->do8BitRegisterRotateRight(&(this->af.parts.hi)); return 8; // RRC A - 8 cycles
        case 0x08: this->do8BitRegisterRotateRight(&(this->bc.parts.hi)); return 8; // RRC B - 8 cycles
        case 0x09: this->do8BitRegisterRotateRight(&(this->bc.parts.lo)); return 8; // RRC C - 8 cycles
        case 0x0A: this->do8BitRegisterRotateRight(&(this->de.parts.hi)); return 8; // RRC D - 8 cycles
        case 0x0B: this->do8BitRegisterRotateRight(&(this->de.parts.lo)); return 8; // RRC E - 8 cycles
        case 0x0C: this->do8BitRegisterRotateRight(&(this->hl.parts.hi)); return 8; // RRC H - 8 cycles
        case 0x0D: this->do8BitRegisterRotateRight(&(this->hl.parts.lo)); return 8; // RRC L - 8 cycles
        // RRC (HL) - 16 cycles
        case 0x0E:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterRotateRight(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Rotate - (RR n) - Rotate n right through carry flag
        case 0x1F: this->do8BitRegisterRotateRight(&(this->af.parts.hi), true); return 8; // RR A - 8 cycles
        case 0x18: this->do8BitRegisterRotateRight(&(this->bc.parts.hi), true); return 8; // RR B - 8 cycles
        case 0x19: this->do8BitRegisterRotateRight(&(this->bc.parts.lo), true); return 8; // RR C - 8 cycles
        case 0x1A: this->do8BitRegisterRotateRight(&(this->de.parts.hi), true); return 8; // RR D - 8 cycles
        case 0x1B: this->do8BitRegisterRotateRight(&(this->de.parts.lo), true); return 8; // RR E - 8 cycles
        case 0x1C: this->do8BitRegisterRotateRight(&(this->hl.parts.hi), true); return 8; // RR H - 8 cycles
        case 0x1D: this->do8BitRegisterRotateRight(&(this->hl.parts.lo), true); return 8; // RR L - 8 cycles
        // RR (HL) - 16 cycles
        case 0x1E:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterRotateRight(&temp, true);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Shift - (SLA n) - Shift n left, Bit 7 to Carry flag
        case 0x27: this->do8BitRegisterShiftLeft(&(this->af.parts.hi)); return 8; // SLA A - 8 cycles
        case 0x20: this->do8BitRegisterShiftLeft(&(this->bc.parts.hi)); return 8; // SLA B - 8 cycles
        case 0x21: this->do8BitRegisterShiftLeft(&(this->bc.parts.lo)); return 8; // SLA C - 8 cycles
        case 0x22: this->do8BitRegisterShiftLeft(&(this->de.parts.hi)); return 8; // SLA D - 8 cycles
        case 0x23: this->do8BitRegisterShiftLeft(&(this->de.parts.lo)); return 8; // SLA E - 8 cycles
        case 0x24: this->do8BitRegisterShiftLeft(&(this->hl.parts.hi)); return 8; // SLA H - 8 cycles
        case 0x25: this->do8BitRegisterShiftLeft(&(this->hl.parts.lo)); return 8; // SLA L - 8 cycles
        // RLC (HL) - 16 cycles
        case 0x26:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterShiftLeft(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Shift - (SRA n) - Shift n right, maintaining MSB, Bit 0 to Carry flag
        case 0x2F: this->do8BitRegisterShiftRight(&(this->af.parts.hi), true); return 8; // SRA A - 8 cycles
        case 0x28: this->do8BitRegisterShiftRight(&(this->bc.parts.hi), true); return 8; // SRA B - 8 cycles
        case 0x29: this->do8BitRegisterShiftRight(&(this->bc.parts.lo), true); return 8; // SRA C - 8 cycles
        case 0x2A: this->do8BitRegisterShiftRight(&(this->de.parts.hi), true); return 8; // SRA D - 8 cycles
        case 0x2B: this->do8BitRegisterShiftRight(&(this->de.parts.lo), true); return 8; // SRA E - 8 cycles
        case 0x2C: this->do8BitRegisterShiftRight(&(this->hl.parts.hi), true); return 8; // SRA H - 8 cycles
        case 0x2D: this->do8BitRegisterShiftRight(&(this->hl.parts.lo), true); return 8; // SRA L - 8 cycles
        // SRA (HL) - 16 cycles
        case 0x2E:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterShiftRight(&temp, true);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Shift - (SRL n) - Shift n right, Bit 0 to Carry flag
        case 0x3F: this->do8BitRegisterShiftRight(&(this->af.parts.hi)); return 8; // SRL A - 8 cycles
        case 0x38: this->do8BitRegisterShiftRight(&(this->bc.parts.hi)); return 8; // SRL B - 8 cycles
        case 0x39: this->do8BitRegisterShiftRight(&(this->bc.parts.lo)); return 8; // SRL C - 8 cycles
        case 0x3A: this->do8BitRegisterShiftRight(&(this->de.parts.hi)); return 8; // SRL D - 8 cycles
        case 0x3B: this->do8BitRegisterShiftRight(&(this->de.parts.lo)); return 8; // SRL E - 8 cycles
        case 0x3C: this->do8BitRegisterShiftRight(&(this->hl.parts.hi)); return 8; // SRL H - 8 cycles
        case 0x3D: this->do8BitRegisterShiftRight(&(this->hl.parts.lo)); return 8; // SRL L - 8 cycles
        // SRL (HL) - 16 cycles
        case 0x3E:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            this->do8BitRegisterShiftRight(&temp);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }

        // Bit - (BIT b, r) - Test bit b in register r
        case 0x40: this->doTestBit(this->bc.parts.hi, 0);                   return 8;  // BIT 0, B - 8 cycles
        case 0x41: this->doTestBit(this->bc.parts.lo, 0);                   return 8;  // BIT 0, C - 8 cycles
        case 0x42: this->doTestBit(this->de.parts.hi, 0);                   return 8;  // BIT 0, D - 8 cycles
        case 0x43: this->doTestBit(this->de.parts.lo, 0);                   return 8;  // BIT 0, E - 8 cycles
        case 0x44: this->doTestBit(this->hl.parts.hi, 0);                   return 8;  // BIT 0, H - 8 cycles
        case 0x45: this->doTestBit(this->hl.parts.lo, 0);                   return 8;  // BIT 0, L - 8 cycles
        case 0x46: this->doTestBit(this->mmu->readMemory(this->hl.reg), 0); return 16; // BIT 0, (HL) - 8 cycles
        case 0x47: this->doTestBit(this->af.parts.hi, 0);                   return 8;  // BIT 0, A - 8 cycles
        case 0x48: this->doTestBit(this->bc.parts.hi, 1);                   return 8;  // BIT 1, B - 8 cycles
        case 0x49: this->doTestBit(this->bc.parts.lo, 1);                   return 8;  // BIT 1, C - 8 cycles
        case 0x4A: this->doTestBit(this->de.parts.hi, 1);                   return 8;  // BIT 1, D - 8 cycles
        case 0x4B: this->doTestBit(this->de.parts.lo, 1);                   return 8;  // BIT 1, E - 8 cycles
        case 0x4C: this->doTestBit(this->hl.parts.hi, 1);                   return 8;  // BIT 1, H - 8 cycles
        case 0x4D: this->doTestBit(this->hl.parts.lo, 1);                   return 8;  // BIT 1, L - 8 cycles
        case 0x4E: this->doTestBit(this->mmu->readMemory(this->hl.reg), 1); return 16; // BIT 1, (HL) - 8 cycles
        case 0x4F: this->doTestBit(this->af.parts.hi, 1);                   return 8;  // BIT 1, A - 8 cycles
        case 0x50: this->doTestBit(this->bc.parts.hi, 2);                   return 8;  // BIT 2, B - 8 cycles
        case 0x51: this->doTestBit(this->bc.parts.lo, 2);                   return 8;  // BIT 2, C - 8 cycles
        case 0x52: this->doTestBit(this->de.parts.hi, 2);                   return 8;  // BIT 2, D - 8 cycles
        case 0x53: this->doTestBit(this->de.parts.lo, 2);                   return 8;  // BIT 2, E - 8 cycles
        case 0x54: this->doTestBit(this->hl.parts.hi, 2);                   return 8;  // BIT 2, H - 8 cycles
        case 0x55: this->doTestBit(this->hl.parts.lo, 2);                   return 8;  // BIT 2, L - 8 cycles
        case 0x56: this->doTestBit(this->mmu->readMemory(this->hl.reg), 2); return 16; // BIT 2, (HL) - 8 cycles
        case 0x57: this->doTestBit(this->af.parts.hi, 2);                   return 8;  // BIT 2, A - 8 cycles
        case 0x58: this->doTestBit(this->bc.parts.hi, 3);                   return 8;  // BIT 3, B - 8 cycles
        case 0x59: this->doTestBit(this->bc.parts.lo, 3);                   return 8;  // BIT 3, C - 8 cycles
        case 0x5A: this->doTestBit(this->de.parts.hi, 3);                   return 8;  // BIT 3, D - 8 cycles
        case 0x5B: this->doTestBit(this->de.parts.lo, 3);                   return 8;  // BIT 3, E - 8 cycles
        case 0x5C: this->doTestBit(this->hl.parts.hi, 3);                   return 8;  // BIT 3, H - 8 cycles
        case 0x5D: this->doTestBit(this->hl.parts.lo, 3);                   return 8;  // BIT 3, L - 8 cycles
        case 0x5E: this->doTestBit(this->mmu->readMemory(this->hl.reg), 3); return 16; // BIT 3, (HL) - 8 cycles
        case 0x5F: this->doTestBit(this->af.parts.hi, 3);                   return 8;  // BIT 3, A - 8 cycles
        case 0x60: this->doTestBit(this->bc.parts.hi, 4);                   return 8;  // BIT 4, B - 8 cycles
        case 0x61: this->doTestBit(this->bc.parts.lo, 4);                   return 8;  // BIT 4, C - 8 cycles
        case 0x62: this->doTestBit(this->de.parts.hi, 4);                   return 8;  // BIT 4, D - 8 cycles
        case 0x63: this->doTestBit(this->de.parts.lo, 4);                   return 8;  // BIT 4, E - 8 cycles
        case 0x64: this->doTestBit(this->hl.parts.hi, 4);                   return 8;  // BIT 4, H - 8 cycles
        case 0x65: this->doTestBit(this->hl.parts.lo, 4);                   return 8;  // BIT 4, L - 8 cycles
        case 0x66: this->doTestBit(this->mmu->readMemory(this->hl.reg), 4); return 16; // BIT 4, (HL) - 8 cycles
        case 0x67: this->doTestBit(this->af.parts.hi, 4);                   return 8;  // BIT 4, A - 8 cycles
        case 0x68: this->doTestBit(this->bc.parts.hi, 5);                   return 8;  // BIT 5, B - 8 cycles
        case 0x69: this->doTestBit(this->bc.parts.lo, 5);                   return 8;  // BIT 5, C - 8 cycles
        case 0x6A: this->doTestBit(this->de.parts.hi, 5);                   return 8;  // BIT 5, D - 8 cycles
        case 0x6B: this->doTestBit(this->de.parts.lo, 5);                   return 8;  // BIT 5, E - 8 cycles
        case 0x6C: this->doTestBit(this->hl.parts.hi, 5);                   return 8;  // BIT 5, H - 8 cycles
        case 0x6D: this->doTestBit(this->hl.parts.lo, 5);                   return 8;  // BIT 5, L - 8 cycles
        case 0x6E: this->doTestBit(this->mmu->readMemory(this->hl.reg), 5); return 16; // BIT 5, (HL) - 8 cycles
        case 0x6F: this->doTestBit(this->af.parts.hi, 5);                   return 8;  // BIT 5, A - 8 cycles
        case 0x70: this->doTestBit(this->bc.parts.hi, 6);                   return 8;  // BIT 6, B - 8 cycles
        case 0x71: this->doTestBit(this->bc.parts.lo, 6);                   return 8;  // BIT 6, C - 8 cycles
        case 0x72: this->doTestBit(this->de.parts.hi, 6);                   return 8;  // BIT 6, D - 8 cycles
        case 0x73: this->doTestBit(this->de.parts.lo, 6);                   return 8;  // BIT 6, E - 8 cycles
        case 0x74: this->doTestBit(this->hl.parts.hi, 6);                   return 8;  // BIT 6, H - 8 cycles
        case 0x75: this->doTestBit(this->hl.parts.lo, 6);                   return 8;  // BIT 6, L - 8 cycles
        case 0x76: this->doTestBit(this->mmu->readMemory(this->hl.reg), 6); return 16; // BIT 6, (HL) - 8 cycles
        case 0x77: this->doTestBit(this->af.parts.hi, 6);                   return 8;  // BIT 6, A - 8 cycles
        case 0x78: this->doTestBit(this->bc.parts.hi, 7);                   return 8;  // BIT 7, B - 8 cycles
        case 0x79: this->doTestBit(this->bc.parts.lo, 7);                   return 8;  // BIT 7, C - 8 cycles
        case 0x7A: this->doTestBit(this->de.parts.hi, 7);                   return 8;  // BIT 7, D - 8 cycles
        case 0x7B: this->doTestBit(this->de.parts.lo, 7);                   return 8;  // BIT 7, E - 8 cycles
        case 0x7C: this->doTestBit(this->hl.parts.hi, 7);                   return 8;  // BIT 7, H - 8 cycles
        case 0x7D: this->doTestBit(this->hl.parts.lo, 7);                   return 8;  // BIT 7, L - 8 cycles
        case 0x7E: this->doTestBit(this->mmu->readMemory(this->hl.reg), 7); return 16; // BIT 7, (HL) - 8 cycles
        case 0x7F: this->doTestBit(this->af.parts.hi, 7);                   return 8;  // BIT 7, A - 8 cycles

        // Bit - (SET b, r) - Set bit b in register r
        case 0xC0: setBit(&(this->bc.parts.hi), 0); return 8; // SET 0, B - 8 cycles
        case 0xC1: setBit(&(this->bc.parts.lo), 0); return 8; // SET 0, C - 8 cycles
        case 0xC2: setBit(&(this->de.parts.hi), 0); return 8; // SET 0, D - 8 cycles
        case 0xC3: setBit(&(this->de.parts.lo), 0); return 8; // SET 0, E - 8 cycles
        case 0xC4: setBit(&(this->hl.parts.hi), 0); return 8; // SET 0, H - 8 cycles
        case 0xC5: setBit(&(this->hl.parts.lo), 0); return 8; // SET 0, L - 8 cycles
        // SET 0, (HL) - 16 cycles
        case 0xC6:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 0);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xC7: setBit(&(this->af.parts.hi), 0); return 8; // SET 0, A - 8 cycles
        case 0xC8: setBit(&(this->bc.parts.hi), 1); return 8; // SET 1, B - 8 cycles
        case 0xC9: setBit(&(this->bc.parts.lo), 1); return 8; // SET 1, C - 8 cycles
        case 0xCA: setBit(&(this->de.parts.hi), 1); return 8; // SET 1, D - 8 cycles
        case 0xCB: setBit(&(this->de.parts.lo), 1); return 8; // SET 1, E - 8 cycles
        case 0xCC: setBit(&(this->hl.parts.hi), 1); return 8; // SET 1, H - 8 cycles
        case 0xCD: setBit(&(this->hl.parts.lo), 1); return 8; // SET 1, L - 8 cycles
        // SET 1, (HL) - 16 cycles
        case 0xCE:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 1);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xCF: setBit(&(this->af.parts.hi), 1); return 8; // SET 1, A - 8 cycles
        case 0xD0: setBit(&(this->bc.parts.hi), 2); return 8; // SET 2, B - 8 cycles
        case 0xD1: setBit(&(this->bc.parts.lo), 2); return 8; // SET 2, C - 8 cycles
        case 0xD2: setBit(&(this->de.parts.hi), 2); return 8; // SET 2, D - 8 cycles
        case 0xD3: setBit(&(this->de.parts.lo), 2); return 8; // SET 2, E - 8 cycles
        case 0xD4: setBit(&(this->hl.parts.hi), 2); return 8; // SET 2, H - 8 cycles
        case 0xD5: setBit(&(this->hl.parts.lo), 2); return 8; // SET 2, L - 8 cycles
        // SET 2, (HL) - 16 cycles
        case 0xD6:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 2);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xD7: setBit(&(this->af.parts.hi), 2); return 8; // SET 2, A - 8 cycles
        case 0xD8: setBit(&(this->bc.parts.hi), 3); return 8; // SET 3, B - 8 cycles
        case 0xD9: setBit(&(this->bc.parts.lo), 3); return 8; // SET 3, C - 8 cycles
        case 0xDA: setBit(&(this->de.parts.hi), 3); return 8; // SET 3, D - 8 cycles
        case 0xDB: setBit(&(this->de.parts.lo), 3); return 8; // SET 3, E - 8 cycles
        case 0xDC: setBit(&(this->hl.parts.hi), 3); return 8; // SET 3, H - 8 cycles
        case 0xDD: setBit(&(this->hl.parts.lo), 3); return 8; // SET 3, L - 8 cycles
        // SET 3, (HL) - 16 cycles
        case 0xDE:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 3);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xDF: setBit(&(this->af.parts.hi), 3); return 8; // SET 3, A - 8 cycles
        case 0xE0: setBit(&(this->bc.parts.hi), 4); return 8; // SET 4, B - 8 cycles
        case 0xE1: setBit(&(this->bc.parts.lo), 4); return 8; // SET 4, C - 8 cycles
        case 0xE2: setBit(&(this->de.parts.hi), 4); return 8; // SET 4, D - 8 cycles
        case 0xE3: setBit(&(this->de.parts.lo), 4); return 8; // SET 4, E - 8 cycles
        case 0xE4: setBit(&(this->hl.parts.hi), 4); return 8; // SET 4, H - 8 cycles
        case 0xE5: setBit(&(this->hl.parts.lo), 4); return 8; // SET 4, L - 8 cycles
        // SET 4, (HL) - 16 cycles
        case 0xE6:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 4);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xE7: setBit(&(this->af.parts.hi), 4); return 8; // SET 4, A - 8 cycles
        case 0xE8: setBit(&(this->bc.parts.hi), 5); return 8; // SET 5, B - 8 cycles
        case 0xE9: setBit(&(this->bc.parts.lo), 5); return 8; // SET 5, C - 8 cycles
        case 0xEA: setBit(&(this->de.parts.hi), 5); return 8; // SET 5, D - 8 cycles
        case 0xEB: setBit(&(this->de.parts.lo), 5); return 8; // SET 5, E - 8 cycles
        case 0xEC: setBit(&(this->hl.parts.hi), 5); return 8; // SET 5, H - 8 cycles
        case 0xED: setBit(&(this->hl.parts.lo), 5); return 8; // SET 5, L - 8 cycles
        // SET 5, (HL) - 16 cycles
        case 0xEE:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 5);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xEF: setBit(&(this->af.parts.hi), 5); return 8; // SET 5, A - 8 cycles
        case 0xF0: setBit(&(this->bc.parts.hi), 6); return 8; // SET 6, B - 8 cycles
        case 0xF1: setBit(&(this->bc.parts.lo), 6); return 8; // SET 6, C - 8 cycles
        case 0xF2: setBit(&(this->de.parts.hi), 6); return 8; // SET 6, D - 8 cycles
        case 0xF3: setBit(&(this->de.parts.lo), 6); return 8; // SET 6, E - 8 cycles
        case 0xF4: setBit(&(this->hl.parts.hi), 6); return 8; // SET 6, H - 8 cycles
        case 0xF5: setBit(&(this->hl.parts.lo), 6); return 8; // SET 6, L - 8 cycles
        // SET 6, (HL) - 16 cycles
        case 0xF6:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 6);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xF7: setBit(&(this->af.parts.hi), 6); return 8; // SET 6, A - 8 cycles
        case 0xF8: setBit(&(this->bc.parts.hi), 7); return 8; // SET 7, B - 8 cycles
        case 0xF9: setBit(&(this->bc.parts.lo), 7); return 8; // SET 7, C - 8 cycles
        case 0xFA: setBit(&(this->de.parts.hi), 7); return 8; // SET 7, D - 8 cycles
        case 0xFB: setBit(&(this->de.parts.lo), 7); return 8; // SET 7, E - 8 cycles
        case 0xFC: setBit(&(this->hl.parts.hi), 7); return 8; // SET 7, H - 8 cycles
        case 0xFD: setBit(&(this->hl.parts.lo), 7); return 8; // SET 7, L - 8 cycles
        // SET 7, (HL) - 16 cycles
        case 0xFE:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            setBit(&temp, 7);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xFF: setBit(&(this->af.parts.hi), 7); return 8; // SET 7, A - 8 cycles

        // Bit - (RES b, r) - Reset bit b in register r
        case 0x80: resetBit(&(this->bc.parts.hi), 0); return 8; // RES 0, B - 8 cycles
        case 0x81: resetBit(&(this->bc.parts.lo), 0); return 8; // RES 0, C - 8 cycles
        case 0x82: resetBit(&(this->de.parts.hi), 0); return 8; // RES 0, D - 8 cycles
        case 0x83: resetBit(&(this->de.parts.lo), 0); return 8; // RES 0, E - 8 cycles
        case 0x84: resetBit(&(this->hl.parts.hi), 0); return 8; // RES 0, H - 8 cycles
        case 0x85: resetBit(&(this->hl.parts.lo), 0); return 8; // RES 0, L - 8 cycles
        // RES 0, (HL) - 16 cycles
        case 0x86:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 0);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0x87: resetBit(&(this->af.parts.hi), 0); return 8; // RES 0, A - 8 cycles
        case 0x88: resetBit(&(this->bc.parts.hi), 1); return 8; // RES 1, B - 8 cycles
        case 0x89: resetBit(&(this->bc.parts.lo), 1); return 8; // RES 1, C - 8 cycles
        case 0x8A: resetBit(&(this->de.parts.hi), 1); return 8; // RES 1, D - 8 cycles
        case 0x8B: resetBit(&(this->de.parts.lo), 1); return 8; // RES 1, E - 8 cycles
        case 0x8C: resetBit(&(this->hl.parts.hi), 1); return 8; // RES 1, H - 8 cycles
        case 0x8D: resetBit(&(this->hl.parts.lo), 1); return 8; // RES 1, L - 8 cycles
        // RES 1, (HL) - 16 cycles
        case 0x8E:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 1);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0x8F: resetBit(&(this->af.parts.hi), 1); return 8; // RES 1, A - 8 cycles
        case 0x90: resetBit(&(this->bc.parts.hi), 2); return 8; // RES 2, B - 8 cycles
        case 0x91: resetBit(&(this->bc.parts.lo), 2); return 8; // RES 2, C - 8 cycles
        case 0x92: resetBit(&(this->de.parts.hi), 2); return 8; // RES 2, D - 8 cycles
        case 0x93: resetBit(&(this->de.parts.lo), 2); return 8; // RES 2, E - 8 cycles
        case 0x94: resetBit(&(this->hl.parts.hi), 2); return 8; // RES 2, H - 8 cycles
        case 0x95: resetBit(&(this->hl.parts.lo), 2); return 8; // RES 2, L - 8 cycles
        // RES 2, (HL) - 16 cycles
        case 0x96:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 2);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0x97: resetBit(&(this->af.parts.hi), 2); return 8; // RES 2, A - 8 cycles
        case 0x98: resetBit(&(this->bc.parts.hi), 3); return 8; // RES 3, B - 8 cycles
        case 0x99: resetBit(&(this->bc.parts.lo), 3); return 8; // RES 3, C - 8 cycles
        case 0x9A: resetBit(&(this->de.parts.hi), 3); return 8; // RES 3, D - 8 cycles
        case 0x9B: resetBit(&(this->de.parts.lo), 3); return 8; // RES 3, E - 8 cycles
        case 0x9C: resetBit(&(this->hl.parts.hi), 3); return 8; // RES 3, H - 8 cycles
        case 0x9D: resetBit(&(this->hl.parts.lo), 3); return 8; // RES 3, L - 8 cycles
        // RES 3, (HL) - 16 cycles
        case 0x9E:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 3);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0x9F: resetBit(&(this->af.parts.hi), 3); return 8; // RES 3, A - 8 cycles
        case 0xA0: resetBit(&(this->bc.parts.hi), 4); return 8; // RES 4, B - 8 cycles
        case 0xA1: resetBit(&(this->bc.parts.lo), 4); return 8; // RES 4, C - 8 cycles
        case 0xA2: resetBit(&(this->de.parts.hi), 4); return 8; // RES 4, D - 8 cycles
        case 0xA3: resetBit(&(this->de.parts.lo), 4); return 8; // RES 4, E - 8 cycles
        case 0xA4: resetBit(&(this->hl.parts.hi), 4); return 8; // RES 4, H - 8 cycles
        case 0xA5: resetBit(&(this->hl.parts.lo), 4); return 8; // RES 4, L - 8 cycles
        // RES 4, (HL) - 16 cycles
        case 0xA6:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 4);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xA7: resetBit(&(this->af.parts.hi), 4); return 8; // RES 4, A - 8 cycles
        case 0xA8: resetBit(&(this->bc.parts.hi), 5); return 8; // RES 5, B - 8 cycles
        case 0xA9: resetBit(&(this->bc.parts.lo), 5); return 8; // RES 5, C - 8 cycles
        case 0xAA: resetBit(&(this->de.parts.hi), 5); return 8; // RES 5, D - 8 cycles
        case 0xAB: resetBit(&(this->de.parts.lo), 5); return 8; // RES 5, E - 8 cycles
        case 0xAC: resetBit(&(this->hl.parts.hi), 5); return 8; // RES 5, H - 8 cycles
        case 0xAD: resetBit(&(this->hl.parts.lo), 5); return 8; // RES 5, L - 8 cycles
        // RES 5, (HL) - 16 cycles
        case 0xAE:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 5);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xAF: resetBit(&(this->af.parts.hi), 5); return 8; // RES 5, A - 8 cycles
        case 0xB0: resetBit(&(this->bc.parts.hi), 6); return 8; // RES 6, B - 8 cycles
        case 0xB1: resetBit(&(this->bc.parts.lo), 6); return 8; // RES 6, C - 8 cycles
        case 0xB2: resetBit(&(this->de.parts.hi), 6); return 8; // RES 6, D - 8 cycles
        case 0xB3: resetBit(&(this->de.parts.lo), 6); return 8; // RES 6, E - 8 cycles
        case 0xB4: resetBit(&(this->hl.parts.hi), 6); return 8; // RES 6, H - 8 cycles
        case 0xB5: resetBit(&(this->hl.parts.lo), 6); return 8; // RES 6, L - 8 cycles
        // RES 6, (HL) - 16 cycles
        case 0xB6:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 6);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xB7: resetBit(&(this->af.parts.hi), 6); return 8; // RES 6, A - 8 cycles
        case 0xB8: resetBit(&(this->bc.parts.hi), 7); return 8; // RES 7, B - 8 cycles
        case 0xB9: resetBit(&(this->bc.parts.lo), 7); return 8; // RES 7, C - 8 cycles
        case 0xBA: resetBit(&(this->de.parts.hi), 7); return 8; // RES 7, D - 8 cycles
        case 0xBB: resetBit(&(this->de.parts.lo), 7); return 8; // RES 7, E - 8 cycles
        case 0xBC: resetBit(&(this->hl.parts.hi), 7); return 8; // RES 7, H - 8 cycles
        case 0xBD: resetBit(&(this->hl.parts.lo), 7); return 8; // RES 7, L - 8 cycles
        // RES 7, (HL) - 16 cycles
        case 0xBE:
        {
            Byte temp = this->mmu->readMemory(this->hl.reg);
            resetBit(&temp, 7);
            this->mmu->writeMemory(this->hl.reg, temp);
            return 16;
        }
        case 0xBF: resetBit(&(this->af.parts.hi), 7); return 8; // RES 7, A - 8 cycles

        default: std::cout << "unknown op: 0x" << std::hex << opcode << std::endl; return 4;
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    if (result > 0xFF)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), CARRY_BIT);
    }

    Word lowerNibble = *reg & 0xF;
    if (lowerNibble + (value & 0xF) > 0xF)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), CARRY_BIT);
    }


    Word lowerTwelve = *reg & 0xFFF;
    if (lowerTwelve + (value & 0xFFF) > 0xFFF)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    if (*reg < toSubtract)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), CARRY_BIT);
    }


    Word lowerNibble = *reg & 0xF;
    if ((toSubtract & 0xF) < lowerNibble)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    if (source < value)
    {
        setBit(&(this->af.parts.lo), CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), CARRY_BIT);
    }


    Word lowerNibble = source & 0xF;
    if ((value & 0xF) < lowerNibble)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    Word lowerNibble = *reg & 0xF;
    if (lowerNibble + 1 > 0xF)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
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
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    Word lowerNibble = *reg & 0xF;
    if (1 < lowerNibble)
    {
        setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    }


    *reg = (Byte) (result & 0xFF);
}

void Cpu::do8BitRegisterSwap(Byte *reg)
{
    // Swaps upper and lower nibbles of register reg
    // Zero flag should be set if the result is zero
    // ALl other flags are reset

    Byte lower = *reg & 0xF;
    *reg = (lower << 4) | (*reg >> 4);

    if (*reg == 0)
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
    resetBit(&(this->af.parts.lo), CARRY_BIT);
}

void Cpu::do8BitRegisterRotateLeft(Byte *reg, bool throughCarry)
{
    // Rotate the bits of register reg left. Bit 7 should be set in the
    // carry flag.
    // Subtract flag should be reset
    // Half carry flag should be reset
    // Zero flag should be set if result is zero
    int bit = getBitVal(*reg, 7);
    *reg <<= 1;
    *reg |= throughCarry ? getBitVal(this->af.parts.lo, CARRY_BIT) : bit;

    if (bit == 1) setBit(&(this->af.parts.lo), CARRY_BIT);
    else resetBit(&(this->af.parts.lo), CARRY_BIT);

    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (*reg == 0) setBit(&(this->af.parts.lo), ZERO_BIT);
    else resetBit(&(this->af.parts.lo), ZERO_BIT);
}

void Cpu::do8BitRegisterShiftLeft(Byte *reg)
{
    // Shift the bits of register reg left. Bit 7 should be set in the
    // carry flag.
    // Subtract flag should be reset
    // Half carry flag should be reset
    // Zero flag should be set if result is zero
    int bit = getBitVal(*reg, 7);
    *reg <<= 1;

    if (bit == 1) setBit(&(this->af.parts.lo), CARRY_BIT);
    else resetBit(&(this->af.parts.lo), CARRY_BIT);

    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (*reg == 0) setBit(&(this->af.parts.lo), ZERO_BIT);
    else resetBit(&(this->af.parts.lo), ZERO_BIT);
}

void Cpu::do8BitRegisterRotateRight(Byte *reg, bool throughCarry)
{
    // Rotate the bits of register reg right. Bit 0 should be set in the
    // carry flag.
    // Subtract flag should be reset
    // Half carry flag should be reset
    // Zero flag should be set if result is zero
    int bit = getBitVal(*reg, 0);
    *reg >>= 1;
    *reg |= ((throughCarry ? getBitVal(this->af.parts.lo, CARRY_BIT) : bit) << 7);

    if (bit == 1) setBit(&(this->af.parts.lo), CARRY_BIT);
    else resetBit(&(this->af.parts.lo), CARRY_BIT);

    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (*reg == 0) setBit(&(this->af.parts.lo), ZERO_BIT);
    else resetBit(&(this->af.parts.lo), ZERO_BIT);
}

void Cpu::do8BitRegisterShiftRight(Byte *reg, bool maintainMsb)
{
    // Shift the bits of register reg right. Bit 0 should be set in the
    // carry flag. If we maintain the most significant bit, ensure
    // it has old value after shift
    // Subtract flag should be reset
    // Half carry flag should be reset
    // Zero flag should be set if result is zero
    int bit = getBitVal(*reg, 0);
    int msb = getBitVal(*reg, 7);
    *reg >>= 1;

    if (maintainMsb)
    {
        *reg |= (msb << 7);
    }

    if (bit == 1) setBit(&(this->af.parts.lo), CARRY_BIT);
    else resetBit(&(this->af.parts.lo), CARRY_BIT);

    resetBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);

    if (*reg == 0) setBit(&(this->af.parts.lo), ZERO_BIT);
    else resetBit(&(this->af.parts.lo), ZERO_BIT);
}

void Cpu::doTestBit(Byte value, int bit)
{
    // Test bit in provided byte
    // If 0, set zero flag, 1 otherwise
    // Reset Subtract flag
    // Set half carry flag
    if (isBitSet(value, bit))
    {
        setBit(&(this->af.parts.lo), ZERO_BIT);
    }
    else
    {
        resetBit(&(this->af.parts.lo), ZERO_BIT);
    }


    setBit(&(this->af.parts.lo), HALF_CARRY_BIT);
    resetBit(&(this->af.parts.lo), SUBTRACT_BIT);
}
