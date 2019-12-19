#ifndef __CPU_H_INCLUDED__
#define __CPU_H_INCLUDED__

#include "mmu.h"
#include "utils.h"

class Cpu {

    public:
        Cpu(Mmu &_mmu) : mmu(mmu) {};

        // Should return number of clock cycles
        // needed for the executing operation
        int execute();

    private:
        Mmu *mmu;

};

#endif
