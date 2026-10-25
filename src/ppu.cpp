#include "ppu.hpp"
#include <iostream>
#include <algorithm>
#include "cpu.hpp"

PPU::PPU(MMU& mmu) : mmu(mmu) {
    framebuffer.fill(0xFF000000);
    palette[0] = 0xFFFFFFFF; // white
    palette[1] = 0xFFAAAAAA; // light grey
    palette[2] = 0xFF555555; // dark grey
    palette[3] = 0xFF000000; // black
}

void PPU::requestInterrupt(int bit) {
    mmu.write(0xFF0F, mmu.read(0xFF0F) | (1 << bit));
}

void PPU::step(int cycles) {
    // Inside your main loop or PPU VBlank check
    if (!(mmu.read(0xFF40) & 0x80)) {     
        if (lcdWasOn){
            lcdWasOn = false;
            modeClock = 0;
            scanline = 0;
            windowLine = 0;

            mode = 0;
            mmu.write(0xFF44, 0);
            mmu.write(0xFF41, mmu.read(0xFF41) & ~0x03);
            framebuffer.fill(palette[0]);
        } 
        return;
    }

    if (!lcdWasOn) {
        lcdWasOn = true;
        modeClock = 0;
        scanline = 0;
        setLY(0);
        setMode(2);
    }

    modeClock += cycles;

    switch (mode) {
        case 0:
            if (modeClock >= 204) {
                modeClock -= 204;
                scanline++;
                setLY(scanline);
                if (scanline == 144 && mode != 1) {
                    setMode(1);
                    requestInterrupt(0);
                }
                else {
                    setMode(2);
                }
            }
            break;
        case 1:
            if (modeClock >= 456) {
                modeClock -= 456;
                scanline++;

                if (scanline > 153) {
                    scanline = 0;
                    windowLine = 0;
                    setMode(2);
                }
                setLY(scanline);
            }
            break;
        case 2:
            if (modeClock >= 80 ) { modeClock -= 80; setMode(3); }
            break;
        case 3:
            if (modeClock >= 172) {
                modeClock -= 172;
                drawScanline(scanline);
                setMode(0);
            }
            break;
    }
}

void PPU::setMode(int m) {
    mode = m;
    uint8_t stat = mmu.memory[0xFF41];
    stat = (stat & ~0x03) | m;
    mmu.memory[0xFF41] = stat;
    updateStatLine();
}

void PPU::setLY(int ly) {
    mmu.memory[0xFF44] = ly;
    uint8_t stat = mmu.memory[0xFF41];
    bool match = (ly == mmu.memory[0xFF45]);
    stat = match ? (stat | 0x04) : (stat & ~ 0x04);
    mmu.memory[0xFF41] = stat;
    updateStatLine();
}

void PPU::drawScanline(int y) {
    uint8_t lcdc = mmu.read(0xFF40);
    uint8_t scy = mmu.read(0xFF42);
    uint8_t scx = mmu.read(0xFF43);
    uint8_t bgp = mmu.read(0xFF47);
    uint8_t wy = mmu.read(0xFF4A);
    int wx = (int)mmu.read(0xFF4B) - 7;

    bool bgOn = lcdc & 0x01;
    bool windowEnabled = (lcdc & 0x20);
    bool renderingWindow = windowEnabled && y >= wy && wx < WIDTH;

    uint16_t tileMapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t windowTileMapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    bool unsignedMode = (lcdc & 0x10);

    int mapY = (y + scy) & 0xFF;
    bool didDrawWindowPixel = false;

    for (int x = 0; x < WIDTH; x++) {
        uint8_t colour = 0;

        if (bgOn) {
            uint16_t base;
            int px, py;
            if(renderingWindow && x >= wx) {
                didDrawWindowPixel = true;
                base = windowTileMapBase;
                px = x - wx;
                py = windowLine;
            }
            else {
                base = tileMapBase;
                px = (x + scx) & 0xFF;
                py = mapY;
            }

            uint8_t tileIndex = mmu.read(base + (py / 8) * 32 + (px / 8));
            uint16_t tileDataAddr = unsignedMode ? 0x8000 + tileIndex * 16 : 0x9000 + (int8_t)tileIndex * 16;

            int row = py % 8;
            uint8_t byte1 = mmu.read(tileDataAddr + row * 2);
            uint8_t byte2 = mmu.read(tileDataAddr + row * 2 + 1);
            int bit = 7 - (px % 8);
            colour = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
        }

        bgColourIndex[x] = colour;
        framebuffer[y * WIDTH + x] = palette[(bgp >> (colour * 2)) & 3];
    }

    if (didDrawWindowPixel) windowLine++;
    drawSprites(y, lcdc);
}

void PPU::drawSprites(int y, uint8_t lcdc) {
    if (!(lcdc & 0x02)) return;
    int height = (lcdc & 0x04) ? 16 : 8;

    struct Spr { int sx; int sy; uint8_t tile; uint8_t attr; int idx;};
    Spr line[10];
    int count = 0;

    for (int i = 0; i < 40 && count < 10; i++) {
        uint16_t oam = 0xFE00 + i * 4;
        int sy = mmu.read(oam) - 16;
        if (y < sy || y >= sy + height) continue;
        line[count++] = { mmu.read(oam + 1) - 8, sy,
                          mmu.read(oam + 2), mmu.read(oam + 3), i };
    }

    std::stable_sort(line, line + count, [](const Spr& a, const Spr& b) {
        if (a.sx != b.sx) return a.sx < b.sx;
        return a.idx < b.idx;
    });

    // static bool done = false;
    // if (!done && (y == 45 || y == 50 || y == 68)) {
    //     printf("--- y=%d count=%d LCDC=%02X\n", y, count, lcdc);
    //     for (int s = 0; s < count; s++)
    //         printf("s=%d x=%d sy=%d tile=%02X attr=%02X\n",
    //             s, line[s].sx, line[s].sy, line[s].tile, line[s].attr);
    // }
    // if (y == 143) done = true;

    for (int x = 0; x < WIDTH; x++) {
        for (int s = 0; s < count; s++) {
            const Spr& sp = line[s];
            int px = x - sp.sx;
            if (px < 0 || px >= 8) continue;

            uint8_t tile = sp.tile, attr = sp.attr;
            int row = y - sp.sy;
            if (attr & 0x40) row = height - 1 - row;
            if (height == 16) tile &= 0xFE;

            uint16_t addr = 0x8000 + tile * 16 + row * 2;
            uint8_t b1 = mmu.read(addr), b2 = mmu.read(addr + 1);
            int bit = (attr & 0x20) ? px : 7 - px;
            uint8_t c = (((b2 >> bit) & 1) << 1) | ((b1 >> bit) & 1);
            if (c == 0) continue;

            if (!((attr & 0x80) && bgColourIndex[x] != 0)) {
                uint8_t pal = mmu.read((attr & 0x10) ? 0xFF49 : 0xFF48);
                framebuffer[y * WIDTH + x] = palette[(pal >> (c * 2)) & 3];
            }
            break;
        }
    }
}

const uint32_t* PPU::getFrameBuffer() const {
    return framebuffer.data();
}

void PPU::updateStatLine() {
    uint8_t stat = mmu.memory[0xFF41];
    bool line = ((stat & 0x08) && mode == 0) || ((stat & 0x10) && mode == 1) || ((stat & 0x20) && mode == 2) || ((stat & 0x40) && (stat & 0x04));
    if (line && !statLine) requestInterrupt(1);
    statLine = line;           
}