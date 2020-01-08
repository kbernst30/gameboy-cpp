#ifndef __GAMEBOY_H_INCLUDED__
#define __GAMEBOY_H_INCLUDED__

#include <SDL2/SDL.h>

#include "cpu.h"
#include "display.h"
#include "mmu.h"
#include "utils.h"

class Gameboy {

    public:
        Gameboy(Mmu *_mmu, Cpu *_cpu, Display *_display) : mmu(_mmu), cpu(_cpu), display(_display) {};

        void run(Byte *cartridge);

    private:
        Mmu *mmu;
        Cpu *cpu;
        Display *display;

        SDL_Window *window;
        SDL_Renderer *renderer;

        int timerCounter = 1024; // initial value, frequency 4096 (4194304/4096)
        int dividerCounter = 0; // Counts up to 255
        int scanlineCounter = 456; // It takes 456 clock cycles to draw one scanline

        bool isClockEnabled();
        void setClockFrequency();
        void updateDividerCounter(int cycles);

        int update();
        void updateGraphics(int cycles);
        void updateTimers(int cycles);

        void doInterrupts();

        bool isLcdEnabled();
        void setLcdStatus();

        // GUI - OpenGL/SDL
        bool createWindow();
        void renderGame();
};

#endif