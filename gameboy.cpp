#include <iostream>
#include <memory>
#include "gameboy.h"

using namespace std;

Gameboy::Gameboy() {
    auto mmu = make_unique<Mmu>();
    // Cpu cpu(mmu);
}

void Gameboy::run() {
    cout << "Gameboy is running";
}
