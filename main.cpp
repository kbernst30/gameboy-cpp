/** DON'T DELETE THIS - WE WILL REUSE LATER **/

// #include <stdlib.h>
// #include <SDL2/SDL.h>

// #define WINDOW_WIDTH 600

// int main()
// {
//     SDL_Event event;
//     SDL_Renderer *renderer;
//     SDL_Window *window;
//     int i;

//     SDL_Init(SDL_INIT_VIDEO);
//     SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_WIDTH, 0, &window, &renderer);
//     SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
//     SDL_RenderClear(renderer);
//     SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
//     for (i = 0; i < WINDOW_WIDTH; ++i)
//         SDL_RenderDrawPoint(renderer, i, i);
//     SDL_RenderPresent(renderer);
//     while (1) {
//         if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
//             break;
//     }
//     SDL_DestroyRenderer(renderer);
//     SDL_DestroyWindow(window);
//     SDL_Quit();
//     return EXIT_SUCCESS;
// }

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <memory>
#include <iostream>
#include <fstream>

#include "cpu.h"
#include "display.h"
#include "gameboy.h"
#include "mmu.h"
#include "utils.h"

using namespace std;

void loadGame(Byte *cartridge, const char *rom)
{
    FILE *in;
    in = fopen(rom, "rb");
    fread(cartridge, 1, 0x200000, in);
    fclose(in);
}

int main()
{
    unique_ptr<Mmu> u_mmu = make_unique<Mmu>();
    unique_ptr<Cpu> u_cpu = make_unique<Cpu>(u_mmu.get());
    unique_ptr<Display> u_display = make_unique<Display>(u_mmu.get());

    Gameboy gb(u_mmu.get(), u_cpu.get(), u_display.get());

    // A gameboy cartridge (ROM) has 0x200000 bytes of memory
    // Not all of this memory is loaded into system memory at
    // one given moment (necessarily). Only 0x8000 bytes are stored
    // in memoery at a given time so store the ROM memory separately
    Byte cartridge[0x200000];
    memset(cartridge, 0, sizeof(cartridge));
    loadGame(cartridge, "tetris.gb");

    // This is just a debug loop to see if data was loaded into
    // memory correctly - print out some instructions (start where PC would be)
    for (int i = 0x100; i < 0x100 + 100; i += 2) {
        // printf("0x%.2x\n", cartridge[i]);
        Word address = (cartridge[i + 1] << 8) | cartridge[i];
        cout << "0x" << std::hex << address << endl;
    }

    // TODO we need to deal with the joypad

    gb.run(cartridge);

    // Display *display = u_display.get();
    // display->render();

    unsigned char i = 255;
    printf("%d\n", i);
    printf("%d\n", (char) i);

    return EXIT_SUCCESS;
}
