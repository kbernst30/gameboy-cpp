#ifndef __GAMEBOY_H_INCLUDED__
#define __GAMEBOY_H_INCLUDED__

#include "cpu.h"
#include "mmu.h"
#include "utils.h"

class Gameboy {

    public:
        Gameboy();

        void run();

    private:
        Cpu cpu;
        Mmu mmu;
};

#endif