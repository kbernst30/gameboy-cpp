#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "mmu.h"
#include "utils.h"

using namespace std;

void Mmu::loadRom(Byte *cartridge)
{
    this->cartridge = cartridge;

    // Load ROM banks 0 and 1 into memory
    for (int i = 0; i < MEMORY_ROM_SIZE; i++)
    {
        this->memory[i] = cartridge[i];
    }

    // Once we load the ROM, we need to determine the current bank mode
    // and set the appropriate flag. Memory address 0x147 specifies the current
    // bank mode. If the value is 0, MBC is 0. If it is 1, 2 or 3 it is MBC1 and
    // 5 or 6 specifies MBC2
    switch (this->memory[ROM_BANKING_MODE_ADDR])
    {
        case 1: this->mbc1 = true; break;
        case 2: this->mbc1 = true; break;
        case 3: this->mbc1 = true; break;
        case 5: this->mbc2 = true; break;
        case 6: this->mbc2 = true; break;
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
    this->currentRamBank = 0;

    // Re-initialize RAM to 0
    memset(&(this->ramBanks), 0, sizeof(this->ramBanks));

    this->romBanking = true;
    this->enableRam = false;
}

Byte Mmu::readMemory(Word address)
{
    // If we are reading from ROM banks, ensure we read from the correct bank
    // We do this instead of swapping memory from cartridge into internal memory
    // TODO Is this the best way? Or should we do the swap?
    if (address >= 0x4000 && address < 0x8000)
    {
        return this->cartridge[(address - 0x4000) + (this->currentRomBank * 0x4000)];
    }

    // If we are reading from RAM than we should get data in appropriate RAM bank
    else if (address >= 0xA000 && address < 0xC000)
    {
        return this->ramBanks[(address - 0xA000) + (this->currentRamBank * RAM_BANK_SIZE)];
    }

    // Otherwise just return what's at memory
    return this->memory[address];
}

void Mmu::writeMemory(Word address, Byte data)
{
    // We cannot write to memory 0x0000 - 0x7FFF
    // As this is read only game data
    if (address < 0x8000)
    {
        cout << "0x" << std::hex << address << " accessed. Handle Banking..." << endl;
        this->handleBanking(address, data);
    }

    // If we are writing to ECHO (E000-FDFF) we must write to working RAM (C000-CFFF) as well
    else if (address >= 0xE000 && address < 0xFE00)
    {
        if (this->enableRam)
        {
            this->memory[address] = data;
            this->writeMemory(address - 0x2000, data);
        }
    }

    // Do not allow writing to restricted area (FEA0-FEFF)
    else if (address >= 0xFEA0 && address < 0xFF00)
    {
        cout << "Attemped to write to restricted address 0x" << std::hex << address << endl;
    }

    // We cannot write here directly - reset to 0
    else if (address == DIVIDER_REGISTER_ADDR)
    {
        this->memory[address] = 0;
    }

    // If we are changing the data of the timer controller, then the timer itself will need
    // to reset to count at the new frequency being set here
    else if (address == TIMER_CONTROLLER_ADDR)
    {
        this->setTimerFrequencyChanged(true);
        this->memory[address] = data;
    }

    // Anywhere else is safe to write
    else
    {
        this->memory[address] = data;
    }
}

void Mmu::handleBanking(Word address, Byte data)
{
    // If the address is between 0x0000 and 0x2000, and ROM Banking is enabled
    // then we attempt RAM enabling
    if (address < 0x2000 && (this->mbc1 || this->mbc2))
    {
        this->doEnableRamBanking(address, data);
    }

    // If the address is between 0x2000 and 0x4000, and ROM banking is enabled
    // then we perform a ROM bank change
    else if (address >= 0x2000 && address < 0x4000 && (this->mbc1 || this->mbc2))
    {
        this->doRomLoBankChange(data);
    }

    // If the address is between 0x4000 and 0x6000 then we perform either
    // a RAM bank change or ROM bank change depending on what RAM/ROM mode
    // is selected
    else if (address >= 0x4000 && address < 0x6000)
    {
        //  MBC2 does not have Rambank so we always use 0 and don't have to change
        if (this->mbc1)
        {
            if (this->romBanking)
            {
                this->doRomHiBankChange(data);
            }
            else
            {
                this->doRamBankChange(data);
            }
        }
    }

    // In MBC1, rom banking is flipped depending on data to signify
    // a RAM banking change instead. If we are writing to an address
    // between 0x6000 and 0x8000 that is how we know if we should change
    // this flag or not
    else if (address >= 0x6000 && address < 0x8000 && this->mbc1)
    {
        this->doChangeRomRamMode(data);
    }
}

void Mmu::doEnableRamBanking(Word address, Byte data)
{
    // By default, the emulator will not write to RAM. A game can request
    // to enable RAM bank writing by attempting a write to ROM <0x2000.
    // We check to see if the lower 4 bits (nibble) of the data being written
    // is 0xA. If it is, we enable RAM bank writing. If it is 0 instead, we
    // disable RAM bank writing. Additionally, for MBC2, if the address byte
    // does not have a zero in bit 4, we do nothing.
    if (this->mbc2 && isBitSet(address, 4)) return;

    // Test the lower 4 bits
    if ((data & 0xF) == 0xA)
    {
        this->enableRam = true;
    }
    else if ((data & 0xF) == 0)
    {
        this->enableRam = false;
    }
}

void Mmu::doRomLoBankChange(Byte data)
{
    // When writing to memory between 0x2000 and 0x4000, we should
    // do the first part of changing ROM bank - which is we change
    // the lower bits of the current ROM bank. If we are using
    // MBC1, we should change the lower 5 bits (0-4) and if we are using
    // MBC2, we change the lower 4 (0-3)

    if (this->mbc1)
    {
        // We need to makesure we don't touch the upper 3 bits here as they
        // Will be handled later
        Byte lower5 = data & 0b11111;
        this->currentRomBank &= 224; // Turn off the lower 5
        this->currentRomBank |= lower5;
    }

    else if (this->mbc2)
    {
        // MBC2 does not ever need the upper bits so we can ignore them
        this->currentRomBank = data & 0xF;
    }

    // ROM Bank 0 is always in memory 0x000 - 0x4000 so we must ensure that
    // This is at least rom bank 1 or more
    if (this->currentRomBank == 0)
    {
        this->currentRomBank++;
    }
}

void Mmu::doRomHiBankChange(Byte data)
{
    // If we write to memory 0x4000 - 0x6000 than we need to deal with the upper
    // bits of the memory bank change - This is only applicable for MBC1
    this->currentRomBank &= 31; // Turn off upper 3 bits
    Byte bits = data & 224;
    this->currentRomBank |= bits;

    if (this->currentRomBank == 0)
    {
        this->currentRomBank++;
    }
}

void Mmu::doRamBankChange(Byte data)
{
    // RAM banks cannot be changed in MBC2 as there will be external RAM on the cartride
    // in these cases. If ROM banking is false though, then we can set the current RAM bank
    // to the lower 2 bits of data
    this->currentRamBank = data & 0x3;
}

void Mmu::doChangeRomRamMode(Byte data)
{
    // If we are in MBC1 and attempt to write to address 0x6000 - 0x8000, then we need to
    // change the romBanking value. IF the least significant bit of data being written is 0,
    // then romBanking should be true. Otherwise, set it to false as we are about to do a
    // RAM bank change. We need to ensure current Ram Bank is set to 0 if we enable ROM
    // banking as the Gameboy can only use RAM bank if romBanking is enabled
    bool isLeastSignificantBitSet = isBitSet(data, 0);
    this->romBanking = !isLeastSignificantBitSet ? true : false;
    if (this->romBanking)
    {
        this->currentRamBank = 0;
    }
}

bool Mmu::isTimerFrequencyChanged()
{
    return this->timerFrequencyChanged;
}

void Mmu::setTimerFrequencyChanged(bool val)
{
    this->timerFrequencyChanged = val;
}

void Mmu::increaseDividerRegister()
{
    // We need this special method to increase the divider register because if a game
    // tries to write to this address directly, it will actually reset the value to 0
    this->memory[DIVIDER_REGISTER_ADDR]++;
}
