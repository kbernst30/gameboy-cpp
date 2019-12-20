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

bool Gameboy::isClockEnabled()
{
    // Bit 2 of the timer controller addr specifies if the timer is enabled
    // or not
    return isBitSet(this->mmu->readMemory(TIMER_CONTROLLER_ADDR), 2);
}

void Gameboy::updateTimers(int cycles)
{
    // this->doDividerRegister(cycles);

    // The clock can be disabled so make sure it is enabled before updating anything
    if (isClockEnabled())
    {
        // Update based on how many cycles passed
        // The timer increments when this hits 0 as that is based on the
        // frequency in which the timer should increment (i.e. 4096hz)
        this->timerCounter -= cycles;

        // If enough cycles have passed, we can update the timer
        if (this->timerCounter <= 0)
        {
            // We need to reset the counter value so timer can increment again at the
            // correct frequency
            // this->setClockFrequency();

            // We need to account for overflow - if overflow then we can write the value
            // that is held in the modulator addr and request Timer Interrupt which is
            // bit 2 of the interrupt register in memory
            // Otherwise we can just increment the timer
            Byte currentTimerValue = this->mmu->readMemory(TIMER_ADDR);
            if (currentTimerValue == 255) {
                this->mmu->writeMemory(TIMER_ADDR, this->mmu->readMemory(TIMER_MODULATOR_ADDR));
                // this->requestInterrupt(2);
            } else {
                this->mmu->writeMemory(TIMER_ADDR, currentTimerValue + 1);
            }
        }
    }
}
