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

        this->updateTimers(cycles);
        // UpdateGraphics(cycles)
        this->doInterrupts();
    }

    // RenderScreen();
}

bool Gameboy::isClockEnabled()
{
    // Bit 2 of the timer controller addr specifies if the timer is enabled
    // or not
    return isBitSet(this->mmu->readMemory(TIMER_CONTROLLER_ADDR), 2);
}

void Gameboy::setClockFrequency()
{
    // This will get the first two bits of the timer controller address so we can
    // set the correct frequency
    Byte clockFrequency = this->mmu->readMemory(TIMER_CONTROLLER_ADDR) & 0x3;

    // The timer counter should be set to the value of the speed of the clock
    // divided by the frequency - therefore it will hit zero at the correct
    // rate
    switch (clockFrequency)
    {
        case 0x0: this->timerCounter = CLOCK_SPEED / 4096   ; break;
        case 0x1: this->timerCounter = CLOCK_SPEED / 262144 ; break;
        case 0x2: this->timerCounter = CLOCK_SPEED / 65536  ; break;
        case 0x3: this->timerCounter = CLOCK_SPEED / 16384  ; break;
    }
}

void Gameboy::updateDividerCounter(int cycles)
{
    // Incrememt the counter by number of cycles
    this->dividerCounter += cycles;
    if (this->dividerCounter >= 255)
    {
        // If we go over 255, reset to 0 and then increase the value of the divider register
        this->dividerCounter = 0;
        this->mmu->increaseDividerRegister();
    }
}

void Gameboy::updateTimers(int cycles)
{
    this->updateDividerCounter(cycles);

    // Before updating timers, we should check if the frequency just changed in
    // memory. If it did, we can reset the timer
    if (this->mmu->isTimerFrequencyChanged()) {
        this->setClockFrequency();
        this->mmu->setTimerFrequencyChanged(false);
    }

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
            this->setClockFrequency();

            // We need to account for overflow - if overflow then we can write the value
            // that is held in the modulator addr and request Timer Interrupt which is
            // bit 2 of the interrupt register in memory
            // Otherwise we can just increment the timer
            Byte currentTimerValue = this->mmu->readMemory(TIMER_ADDR);
            if (currentTimerValue == 255) {
                this->mmu->writeMemory(TIMER_ADDR, this->mmu->readMemory(TIMER_MODULATOR_ADDR));
                this->cpu->requestInterrupt(2);
            } else {
                this->mmu->writeMemory(TIMER_ADDR, currentTimerValue + 1);
            }
        }
    }
}

void Gameboy::doInterrupts()
{
    // Only take action on interrupts if the master switch is enabled
    if (this->cpu->isInterruptMaster())
    {
        // Check for pending interrupts, and if they are enabled
        Byte pendingInterrupts = this->mmu->readMemory(INTERRUPT_REQUEST_ADDR);
        if (pendingInterrupts > 0)
        {
            // If we have any interrupts pending, check if they are enabled
            Byte enabledInterrupts = this->mmu->readMemory(INTERRUPT_ENABLED_REGISTER);
            for (int i = 0; i < 5; i++)
            {
                // This loop will ensure we can test every bit that an interrupt can be, in order of priority
                if (isBitSet(pendingInterrupts, i) && isBitSet(enabledInterrupts, i))
                {
                    this->cpu->serviceInterrupt(i);
                }
            }
        }
    }
}
