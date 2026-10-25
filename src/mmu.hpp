#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <vector>


enum class Button {Right, Left, Up, Down, A, B, Select, Start };
class Timer;
class APU;

class MMU {
    public:
        void connectTimer(Timer* t) { timer = t; }
        void connectAPU(APU* a) { apu = a; };
        std::array<uint8_t, 0x10000> memory{};
        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t val);
        bool loadROM(const std::string& path);
        void initPostBoot(); 
        void setButton(Button b, bool pressed);
        void saveRam(const std::string& path);
        void loadRam(const std::string& path);
        std::string serialLog;
    private:
        uint8_t joypadSelect = 0x30;
        uint8_t dpadState = 0x0F;
        uint8_t buttonState = 0x0f;
        
        Timer* timer = nullptr;
        APU* apu = nullptr;
        std::vector<uint8_t> rom;
        bool mbc1 = false;
        size_t rombanks = 2;
        int mbcType = 0;
        size_t rombank = 1, rombank0 = 0, rambank = 0; 
        bool ramEnabled = false;
        int mbc1Lo = 1, mbc1Hi = 0, mbc1Mode = 0;
        bool hasBattery = false;
        std::vector<uint8_t> cartRam;

        uint8_t readJoypad() const;
        void writeMapper(uint16_t addr, uint8_t val);
};