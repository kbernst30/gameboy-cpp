/**
 *
 * MEMORY INFO
 * 0000-3FFF 16KB ROM Bank 00 (in cartridge, fixed at bank 00)
 * 4000-7FFF 16KB ROM Bank 01..NN (in cartridge, switchable bank number)
 * 8000-9FFF 8KB Video RAM (VRAM) (switchable bank 0-1 in CGB Mode)
 * A000-BFFF 8KB External RAM (in cartridge, switchable bank, if any)
 * C000-CFFF 4KB Work RAM Bank 0 (WRAM)
 * D000-DFFF 4KB Work RAM Bank 1 (WRAM) (switchable bank 1-7 in CGB Mode)
 * E000-FDFF Same as C000-DDFF (ECHO) (typically not used)
 * FE00-FE9F Sprite Attribute Table (OAM)
 * FEA0-FEFF Not Usable
 * FF00-FF7F I/O Ports
 * FF80-FFFE High RAM (HRAM) - This is where stack data is
 * FFFF Interrupt Enable Register
 *
 **/

#ifndef __MMU_H_INCLUDED__
#define __MMU_H_INCLUDED__

#include "utils.h"

class Mmu {

    public:
        // Load ROM data into memory
        void loadRom(Byte *cartridge);

        // Reset MMU to initial state
        void reset();

        Byte readMemory(Word address);
        void writeMemory(Word address, Byte data);

    private:
        Byte *cartridge;
        Byte memory[MEMORY_SIZE];

        // Memory banking modes
        bool mbc1 = false;
        bool mbc2 = false;

        // Current bank in switchable memory (0x4000 - 0x7FFF)
        // The default state will be 1
        int currentRomBank = 1;

        // Initialize data for RAM, using an array with the size
        // times the maximum number of banks we have
        int ramBanks[MAXIMUM_RAM_BANKS * RAM_BANK_SIZE];
        int currentRamBank = 0;

        bool romBanking = true;
        bool enableRam = false;

        void handleBanking(Word address, Byte data);
        void doEnableRamBanking(Word address, Byte data);
        void doRomLoBankChange(Byte data);
        void doRomHiBankChange(Byte data);
        void doRamBankChange(Byte data);
        void doChangeRomRamMode(Byte data);
};

#endif
