#include <iostream>

#include "mmu.h"
#include "utils.h"

using namespace std;

void Mmu::loadRom(Byte *cartridge)
{
    for (int i = 0; i < MEMORY_ROM_SIZE; i++)
    {
        this->memory[i] = cartridge[i];
    }

    // Once we load the ROM, we need to determine the current bank mode
    // and set the appropriate flag. Memory address 0x147 specifies the current
    // bank mode. If the value is 0, MBC is 0. If it is 1, 2 or 3 it is MBC1 and
    // 5 or 6 specifies MBC2
    switch (this->memory[0x147])
    {
        case 1: this->mbc1 = true; break;
        case 2: this->mbc1 = true; break;
        case 3: this->mbc1 = true; break;
        case 5: this->mbc1 = true; break;
        case 6: this->mbc1 = true; break;
        default: break;
    }
}

void Mmu::reset()
{
    // This is the initial state of the Mmu
    this->memory[0xFF05] = 0x00;
    this->memory[0xFF06] = 0x00;
    this->memory[0xFF07] = 0x00;
    this->memory[0xFF10] = 0x80;
    this->memory[0xFF11] = 0xBF;
    this->memory[0xFF12] = 0xF3;
    this->memory[0xFF14] = 0xBF;
    this->memory[0xFF16] = 0x3F;
    this->memory[0xFF17] = 0x00;
    this->memory[0xFF19] = 0xBF;
    this->memory[0xFF1A] = 0x7F;
    this->memory[0xFF1B] = 0xFF;
    this->memory[0xFF1E] = 0xBF;
    this->memory[0xFF20] = 0xFF;
    this->memory[0xFF21] = 0x00;
    this->memory[0xFF22] = 0x00;
    this->memory[0xFF23] = 0xBF;
    this->memory[0xFF24] = 0x77;
    this->memory[0xFF25] = 0xF3;
    this->memory[0xFF26] = 0xF1;
    this->memory[0xFF40] = 0x91;
    this->memory[0xFF42] = 0x00;
    this->memory[0xFF43] = 0x00;
    this->memory[0xFF45] = 0x00;
    this->memory[0xFF47] = 0xFC;
    this->memory[0xFF48] = 0xFF;
    this->memory[0xFF49] = 0xFF;
    this->memory[0xFF4A] = 0x00;
    this->memory[0xFF4B] = 0x00;
    this->memory[0xFFFF] = 0x00;

    this->currentRomBank = 1;
}

void Mmu::writeMemory(Word address, Byte data)
{
    // We cannot write to memory 0x0000 - 0x7FFF
    // As this is read only game data
    if (address < 0x8000)
    {
        cout << "Attemped to write to ROM address 0x" << std::hex << address << endl;
    }

    // If we are writing to ECHO (E000-FDFF) we must write to working RAM (C000-CFFF) as well
    else if (address >= 0xE000 && address < 0xFE00)
    {
        this->memory[address] = data;
        this->writeMemory(address - 0x2000, data);
    }

    // Do not allow writing to restricted area (FEA0-FEFF)
    else if (address >= 0xFEA0 && address < 0xFF00)
    {
        cout << "Attemped to write to restricted address 0x" << std::hex << address << endl;
    }

    // Anywhere else is safe to write
    else
    {
        this->memory[address] = data;
    }
}
