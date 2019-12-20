#ifndef __GAMEBOY_H_INCLUDED__
#define __GAMEBOY_H_INCLUDED__

#include "cpu.h"
#include "mmu.h"
#include "utils.h"

class Gameboy {

    public:
        Gameboy(Mmu *_mmu, Cpu *_cpu) : cpu(_cpu), mmu(_mmu) {};

        void run(Byte *cartridge);

    private:
        Cpu *cpu;
        Mmu *mmu;
        Byte *cartridge;

        int timerCounter = 1024; // initial value, frequency 4096 (4194304/4096)

        bool isClockEnabled();

        void update();
        void updateTimers(int cycles);
};

#endif