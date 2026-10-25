#include "mmu.hpp"
#include <iostream>
#include <fstream>
#include <iterator>
#include "timer.hpp"
#include "apu.hpp"


void MMU::initPostBoot() {
    write(0xFF00, 0xCF);
    write(0xFF05, 0x00);
    write(0xFF07, 0xF8);
    write(0xFF0F, 0xE1);
    write(0xFF26, 0xF1);
    write(0xFF25, 0xF3);
    write(0xFF24, 0x77);
    write(0xFF40, 0x91);
    write(0xFF41, 0x85);
    write(0xFF42, 0x00);
    write(0xFF43, 0x00);
    write(0xFF45, 0x00);
    write(0xFF47, 0xFC);
    write(0xFF48, 0xFF);
    write(0xFF49, 0xFF);
    write(0xFF4A, 0x00);
    write(0xFF4B, 0x00);
    write(0xFFFF, 0x00);
}

uint8_t MMU::readJoypad() const {
    uint8_t result = 0xCF | joypadSelect;
    if(!(joypadSelect & 0x10)) result &= (0xF0 | dpadState);
    if(!(joypadSelect & 0x20)) result &= (0xF0 | buttonState);
    return result;
}

void MMU::setButton(Button b, bool pressed) {
    uint8_t before = readJoypad() & 0x0F;

    uint8_t* group = (b <= Button::Down ) ? &dpadState : &buttonState;
    int bit = 0;
    switch (b) {
        case Button::Right: case Button::A: bit = 0; break;
        case Button::Left: case Button::B: bit = 1; break;
        case Button::Up: case Button::Select: bit = 2; break;
        case Button::Down: case Button::Start: bit = 3; break;
    }
    if (pressed) * group &= ~(1 << bit);
    else *group |= (1 << bit);

    uint8_t after = readJoypad() & 0x0F;
    if (before & ~after) memory[0xFF0F] |= 0x10;
}

uint8_t MMU::read(uint16_t addr) {
    if (addr >= 0xE000 && addr < 0xFE00) addr -= 0x2000;

    if (addr < 0x4000) {
        size_t off = rombank0 * 0x4000 + addr;
        return off < rom.size() ? rom[off] : 0xFF;
    }

    if (addr < 0x8000) {
        size_t off = rombank * 0x4000 + (addr - 0x4000);
        return off < rom.size() ? rom[off] : 0xFF;
    }

    if (addr >= 0xA000 && addr < 0xC000) {
        if (!ramEnabled || cartRam.empty()) return 0xFF;
        if (mbcType == 3 && rambank >= 8) return 0;
        size_t idx = (rambank * 0x2000 + (addr - 0xA000)) % cartRam.size();
        return cartRam[idx];
    }

    if (addr >= 0xFF10 && addr <= 0xFF3F && apu) return apu->readRegister(addr);
    if (addr == 0xFF00) return readJoypad();


    return memory[addr];
}

void MMU::write(uint16_t addr, uint8_t val) {
    if (addr >= 0xE000 && addr < 0xFE00) addr -= 0x2000;

    if (addr >= 0xFF10 && addr <= 0xFF3F && apu) { 
        apu->writeRegister(addr, val); 
        return;
    }
    
    if (addr == 0xFF04) {
        if (timer) timer->resetDiv();
        else memory[addr] = 0;
        return;
    }

    if (addr < 0x8000) {
        writeMapper(addr, val);
        return;
    }
    if (addr == 0xFF44) return;

    if (addr == 0xFF41) {
        memory[addr] = (memory[addr] & 0x07) | (val & 0xF8);
        return;
    }

    if (addr == 0xFF46) {
        uint16_t src = val << 8;
        for (int i=0; i < 0xA0; i++) memory[0xFE00 + i] = read(src + i); return;
    }

    if (addr == 0xFF02) {
        memory[addr] = val; 
        if (val == 0x81) {
            char ch = (char)memory[0xFF01];
            serialLog += ch;
            std::cout << ch << std::flush;
            memory[0xFF02] &= 0x7f;
        }
        return;
    }

    if (addr == 0xFF00) { 
        joypadSelect = val & 0x30; 
        return; 
    }

    if (addr >= 0xA000 && addr < 0xC000) {
        if (!ramEnabled || cartRam.empty()) return;
        if (mbcType == 3 && rambank >= 8) return;
        size_t idx = (rambank * 0x2000 + (addr - 0xA000)) % cartRam.size();
        cartRam[idx] = val;
        return;
    }

    memory[addr] = val;
}

void MMU::writeMapper(uint16_t addr, uint8_t val) {
    switch(mbcType) {
        case 1:
            if (addr < 0x2000) ramEnabled = (val & 0x0F) == 0x0A;
            else if (addr < 0x4000) {
                mbc1Lo = val & 0x1F;
                if (mbc1Lo == 0) mbc1Lo = 1;
            }
            else if (addr < 0x6000) mbc1Hi = val & 0x03;
            else mbc1Mode = val & 1;
            rombank = ((mbc1Hi << 5)  | mbc1Lo)  % rombanks;
            rombank0 = mbc1Mode ? ((mbc1Hi << 5) % rombanks)  : 0;
            rambank = mbc1Mode ? mbc1Hi : 0;
            break;
        case 3:
            if (addr < 0x2000) ramEnabled = (val & 0x0F) == 0x0A;
            else if (addr < 0x4000) { size_t b = val & 0x7F; if (b == 0) b = 1; rombank = b % rombanks; }
            else if (addr < 0x6000) rambank = val;
            break;
        case 5:
            if (addr < 0x2000) ramEnabled = (val & 0x0F) == 0x0A;
            else if (addr < 0x3000) rombank = ((rombank & 0x100) | val) % rombanks;
            else if (addr < 0x4000) rombank = ((rombank & 0xFF) | ((val & 1) << 8)) % rombanks;
            else if (addr < 0x6000) rambank = val & 0x0F;
            break;
    }
}

void MMU::saveRam(const std::string& path) {
    if (!hasBattery || cartRam.empty()) return;
    std::ofstream f(path, std::ios::binary);
    f.write((const char*)cartRam.data(), cartRam.size());
}

void MMU::loadRam(const std::string& path) {
    if (!hasBattery || cartRam.empty()) return;
    std::ifstream f(path, std::ios::binary);
    if (f) f.read((char*)cartRam.data(), cartRam.size());
}

bool MMU::loadROM(const std::string& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        std::cout << "File cannot be read" << "\n";
        return false;
    }
    rom.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

    if(rom.size() < 0x150) {
        std::cout << "ROM too small\n";
        return false;
    }

    uint8_t type = rom[0x147];
    printf("type=%02X mbcType=%d\n", type, mbcType);
    mbcType = 0;
    if (type >= 0x01 && type <= 0x03) mbcType = 1;
    else if (type >= 0x0F && type <= 0x13) mbcType = 3;
    else if (type >= 0x19 && type <= 0x1E) mbcType = 5;
    hasBattery = (type == 0x03 || type == 0x06 || type == 0x09 || type == 0x0F || type == 0x10 || type == 0x13 || type == 0x1B || type == 0x1E);
    
    static const size_t ramSizes[] = {0, 0x800, 0x2000, 0x8000, 0x20000, 0x10000};
    uint8_t rc = rom[0x149];
    cartRam.assign(rc < 6 ? ramSizes[rc] : 0, 0);

    rombanks = rom.size() / 0x4000;
    if (rombanks < 2) rombanks = 2; 
    rombank = 1; 
    rombank0 = 0; 
    rambank = 0; 
    ramEnabled = false; 
    mbc1Lo = 1; mbc1Hi = 0; mbc1Mode = 0;
    return true;
}