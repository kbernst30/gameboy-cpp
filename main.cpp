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
    fread(cartridge, 1, CARTRIDGE_SIZE, in);
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
    Byte cartridge[CARTRIDGE_SIZE];
    memset(cartridge, 0, sizeof(cartridge));
    loadGame(cartridge, "rom/instr_timing/instr_timing.gb");

    // This is just a debug loop to see if data was loaded into
    // memory correctly - print out some instructions (start where PC would be)
    // for (int i = 0x100; i < 0x100 + 100; i += 2) {
    //     // printf("0x%.2x\n", cartridge[i]);
    //     Word address = (cartridge[i + 1] << 8) | cartridge[i];
    //     cout << "0x" << std::hex << address << endl;
    // }

    // TODO we need to deal with the joypad

    gb.run(cartridge);

    return EXIT_SUCCESS;
}
