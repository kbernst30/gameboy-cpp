#include <iostream>
#include <memory>
#include <SDL2/SDL.h>

#include "gameboy.h"

using namespace std;

void Gameboy::run(Byte *cartridge) {
    cout << "Gameboy is running" << endl;

    this->cartridge = cartridge;

    // Reset state of the Gameboy
    this->cpu->reset();
    this->mmu->reset();

    this->createWindow();

    float fps = 59.73;

    // ~60fps - This is the amount of times the "update" should execute a second
	float interval = 1000 / fps;

    unsigned int time = SDL_GetTicks();


    // TODO call update 60 times a second (i.e. 60fps)
    SDL_Event event;
    while (true)
    {
        unsigned int currentTime = SDL_GetTicks();

        if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
            break;

        if (time + interval < currentTime)
        {
            time = currentTime;
            std::cout << SDL_GetTicks() << std::endl;
            this->update();
        }
    }

    // SDL_DestroyRenderer(renderer);
    // SDL_DestroyWindow(window);
    SDL_Quit();
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
        this->updateGraphics(cycles);
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

void Gameboy::updateGraphics(int cycles)
{
    this->setLcdStatus();

    if (this->isLcdEnabled())
    {
        // We only update the counter if the LCD is enabled
        this->scanlineCounter -= cycles;
    }

    if (this->scanlineCounter <= 0)
    {
        // If the coutner has coutned down (from 456) then it is time to
        // draw the next scanline
        this->mmu->updateCurrentScanline();
        Byte currentScanline = this->mmu->readMemory(CURRENT_SCANLINE_ADDR);

        // We need to reset our coutner for the next scanline
        this->scanlineCounter = 456;

        if (currentScanline == 144)
        {
            // There are 144 visible scanlines (i.e. scanlines 0 - 143)
            // If we are moving past that scanline, we are not drawing as
            // it is an invisible scanline. This is the vertical blank period
            // and we need an interrupt to handle it (which is bit 0 of the)
            // the interrupt request register
            this->cpu->requestInterrupt(0);
        }
        else if (currentScanline > MAX_SCANLINES)
        {
            // We have gone past the range of scanlines in this case, meaning we
            // need to reset back to 0
            this->mmu->resetCurrentScanline();
        }
        else
        {
            // We are within an appropriate scanline range (i.e. 0 - 143) which
            // means we can draw
            this->display->drawScanline();
        }
    }
}

bool Gameboy::isLcdEnabled()
{
    Byte lcdControl = this->mmu->readMemory(LCD_CONTROL_ADDR);
    return isBitSet(lcdControl, 7); // Bit 7 specified is LCD is enabled
}

void Gameboy::setLcdStatus()
{
    Byte lcdStatus = this->mmu->readMemory(LCD_STATUS_ADDR);

    if (!isLcdEnabled())
    {
        // If the LCD is disabled, the LCD Status should be in mode 1 (V-Blank)
        // and we should ensure we reset the scaline
        this->scanlineCounter = 456;
        this->mmu->resetCurrentScanline();

        lcdStatus &= 0b11111100; // Turn off bits 0 and 1
        setBit(&lcdStatus, 0); // Bit 0 should be set for mode 1
        this->mmu->writeMemory(LCD_STATUS_ADDR, lcdStatus);
        return;
    }

    Byte currentScanline = this->mmu->readMemory(CURRENT_SCANLINE_ADDR);
    Byte lcdMode = lcdStatus & 0x3; // Get the first two bits to determine LCD Mode
    Byte newLcdMode = 0;
    bool shouldRequestInterrupt = false;

    if (currentScanline >= 144)
    {
        // We are in one of our invisible scanlines, which is Vertical Blank period
        // so we should set the mode to V-Blank (mode 1)
        newLcdMode = 1;
        setBit(&lcdStatus, 0);
        resetBit(&lcdStatus, 1);
        shouldRequestInterrupt = isBitSet(lcdStatus, 4); // Bit 4 specifies if V-Blank interrupt is enabled
    }
    else
    {
        // If we are drawing a current scanline, we should be cycling through the other LCD modes
        // The cycle starts for a new scanline in mode 2 (Searching Sprite Atts), then after 80 cycles of
        // the 456 we are counting, we move to mode 3 (Transferring data to LCD driver). Mode 3 will take
        // another 172 cycles, and then we move to mode 0 (H-Blank). When the counter has gone below 0,
        // we will have moved to another scanline and we shuold start back at mode 2
        if (this->scanlineCounter >= 456 - 80)
        {
            // We are in Mode 2 here
            newLcdMode = 2;
            resetBit(&lcdStatus, 0);
            setBit(&lcdStatus, 1);
            shouldRequestInterrupt = isBitSet(lcdStatus, 5); // Bit 5 specifies if Searchiing interrupt is enabled
        }
        else if (this->scanlineCounter < 456 - 80 && this->scanlineCounter >= 456 - 80 - 172)
        {
            // We are in mode 3 here
            newLcdMode = 3;
            setBit(&lcdStatus, 1);
            setBit(&lcdStatus, 1);
        }
        else
        {
            // We are in mode 0 here
            newLcdMode = 0;
            resetBit(&lcdStatus, 0);
            resetBit(&lcdStatus, 1);
            shouldRequestInterrupt = isBitSet(lcdStatus, 3); // Bit 4 specifies if H-Blank interrupt is enabled
        }
    }

    // If we are switching modes and we should request an interrupt, do it
    if (newLcdMode != lcdMode && shouldRequestInterrupt)
    {
        // Interrupt bit 1 is for LCD interrupt
        this->cpu->requestInterrupt(1);
    }

    // If the current scanline is the same as the value in 0xFF45, then
    // we should set the coincidence flag. And appropriately request an
    // LCD interrupt IF it is enabled (bit 6)
    if (currentScanline == this->mmu->readMemory(0xFF45))
    {
        // Set coincidence flag
        setBit(&lcdStatus, 2);
        if (isBitSet(lcdStatus, 6))
        {
            this->cpu->requestInterrupt(1);
        }
    }
    else
    {
        resetBit(&lcdStatus, 2);
    }

    // Finally, update the status now in memory
    this->mmu->writeMemory(CURRENT_SCANLINE_ADDR, lcdStatus);
}

bool Gameboy::createWindow()
{
    if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		return false;
	}

	SDL_Event event;
    SDL_Renderer *renderer;
    SDL_Window *window;
    int i;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, 0, &window, &renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (i = 0; i < SCREEN_WIDTH * 2; ++i)
        SDL_RenderDrawPoint(renderer, i, i);
    SDL_RenderPresent(renderer);
}