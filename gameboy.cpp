#include <iostream>
#include <memory>
#include "gameboy.h"

using namespace std;

void Gameboy::run(Byte *cartridge) {
    cout << "Gameboy is running" << endl;

    this->cartridge = cartridge;

    // Reset state of the Gameboy
    this->cpu->reset();
    this->mmu->reset();

    // TODO call update 60 times a second (i.e. 60fps)
}

void Gameboy::update() {
    // This is the main execution of a "frame"
    // We are targeting 60 FPS
    // The goal here is to run the CPU and
    // execute as many instructions per frame as we can
    // Each Instruction takes a certain number of clock cycles and
    // We have a maximum number of cycles we can execute per frame (see utils.h)
    // As soon as we have executed as many cycles as we can for the frame, we can
    // render the screen

    int cycles;
    while (cycles < MAX_CYCLES_PER_FRAME)
    {
        cycles += this->cpu->execute();

        // UpdateTimers(cycles)
        // UpdateGraphics(cycles)
        // DoInterrupts();
    }

    // RenderScreen();
}
