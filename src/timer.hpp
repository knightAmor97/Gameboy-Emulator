#pragma once
#include <cstdint>
#include "mmu.hpp"

class Timer {
    private:
        MMU& mmu;
        uint16_t divCounter = 0;
        int timerCounter = 0;

    public:
        explicit Timer (MMU& m) : mmu(m) {}

        void step(int cycles) {
            for (int i = 0; i < cycles; i++) {
                divCounter++;

                uint8_t tac = mmu.read(0xFF07);
                if (tac & 0x04) {
                    static const int periods[4] = {1024, 16, 64, 256};
                    timerCounter++;
                    if (timerCounter >= periods[tac & 0x03]) {
                        timerCounter = 0;
                        uint8_t timer = mmu.read(0xFF05);
                        if (timer == 0xFF) {
                            mmu.write(0xFF05, mmu.read(0xFF06));
                            mmu.write(0xFF0F, mmu.read(0xFF0F) | 0x04);
                        }
                        else {
                            mmu.write(0xFF05, timer + 1);
                        }
                    }
                }  
            }
            mmu.memory[0xFF04] = static_cast<uint8_t>(divCounter >> 8);
        }

        void resetDiv() {
            divCounter = 0;
            mmu.memory [0xFF04] = 0;
        }
};