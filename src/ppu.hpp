#pragma once
#include <cstdint>
#include <array>
#include <iostream>
#include "mmu.hpp"

class PPU {
    public:
        static constexpr int WIDTH = 160;
        static constexpr int HEIGHT = 144;
        PPU(MMU& mmu);
        void step(int cycles);
        const uint32_t* getFrameBuffer() const;
        int getScanline() { return scanline; }

    private:
        MMU& mmu;

        std::array<uint32_t, WIDTH * HEIGHT> framebuffer;
        std::array<uint32_t, 4> palette;
        std::array<uint8_t, WIDTH> bgColourIndex;


        int mode = 0;
        int modeClock = 0;
        int scanline = 0;
        int windowLine = 0;
        bool lcdWasOn = false;
        int windowLineCounter = 0;
        bool statLine = false;


        void setMode(int m);
        void setLY(int ly);
        void requestInterrupt(int bit);
        void drawScanline(int y);
        void drawSprites(int y, uint8_t lcdc);
        void updateStatLine();
};