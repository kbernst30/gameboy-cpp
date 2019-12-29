#ifndef __GAMEBOY_H_INCLUDED__
#define __GAMEBOY_H_INCLUDED__

#include "cpu.h"
#include "display.h"
#include "mmu.h"
#include "utils.h"

class Gameboy {

    public:
        Gameboy(Mmu *_mmu, Cpu *_cpu, Display *_display) : cpu(_cpu), mmu(_mmu), display(_display) {};

        void run(Byte *cartridge);

    private:
        Cpu *cpu;
        Display *display;
        Mmu *mmu;
        Byte *cartridge;

        int timerCounter = 1024; // initial value, frequency 4096 (4194304/4096)
        int dividerCounter = 0; // Counts up to 255
        int scanlineCounter = 456; // It takes 456 clock cycles to draw one scanline

        bool isClockEnabled();
        void setClockFrequency();
        void updateDividerCounter(int cycles);

        void update();
        void updateGraphics(int cycles);
        void updateTimers(int cycles);

        void doInterrupts();

        bool isLcdEnabled();
        void setLcdStatus();
};

#endif