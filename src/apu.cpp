#include <cstdint>
#include <array>
#include <vector>
#include <iostream>
#include <SDL2/SDL.h>
#include <algorithm>
#include "mmu.hpp"
#include "apu.hpp"

APU::APU(MMU& m, int rate) 
    : mmu(m), sampleRate(rate) {
        cyclesPerSample = 4194304.0 / sampleRate;
}

void APU::step(int cycles) {
    if (!enabled) return; 

    for (int i = 0; i < cycles; i++) {
        tickPulse(channel1);
        tickPulse(channel2);
        tickWave();
        tickNoise();

        if (++frameSequencerCounter >= 8192) {
            frameSequencerCounter = 0;
            switch (frameSequencerStep) {
                case 0: clockLength(); break;
                case 2: clockLength(); clockSweep(); break;
                case 4: clockLength(); break;
                case 6: clockLength(); clockSweep(); break;
                case 7: clockEnvelope(); break;
            }
            frameSequencerStep = (frameSequencerStep + 1) & 7;
        }

        sampleAccumulator += 1.0;
        if (sampleAccumulator >= cyclesPerSample) {
            sampleAccumulator -= cyclesPerSample;
            generateSample();
        }
    }
}

static const uint8_t DUTY[4][8] = {
    {0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,1,1,1},
    {0,1,1,1,1,1,1,0},
};

void APU::tickPulse(PulseChannel& ch) {
    if (--ch.timer <= 0) {
        ch.timer = (2048 - ch.frequency) * 4;
        ch.dutyPosition = (ch.dutyPosition + 1) & 7;
    }
}

void APU::tickWave() {
    if (--channel3.timer <= 0) {
        channel3.timer = (2048 - channel3.frequency) * 2;
        channel3.position = (channel3.position + 1) & 31;
    }
}

void APU::tickNoise() {
    if (--channel4.timer <=0) {
        static const int divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
        channel4.timer = divisors[channel4.divisorCode] << channel4.clockShift;
        int bit = (channel4.lfsr ^(channel4.lfsr >> 1)) & 1;
        channel4.lfsr = (channel4.lfsr >> 1) | (bit << 14);
        if (channel4.narrowMode) channel4.lfsr = (channel4.lfsr & ~0x40) | (bit << 6);
    }
}

void APU::clockLength() {
    auto lenTick = [](bool en, bool& lenEn, auto& len, bool& chEn) {
        if (lenEn && len > 0 && --len == 0) chEn = false;
    };
    if (channel1.lengthEnabled && channel1.length > 0 && --channel1.length == 0) channel1.enabled = false;
    if (channel2.lengthEnabled && channel2.length > 0 && --channel2.length == 0) channel2.enabled = false;
    if (channel3.lengthEnabled && channel3.length > 0 && --channel3.length == 0) channel3.enabled = false;
    if (channel4.lengthEnabled && channel4.length > 0 && --channel4.length == 0) channel4.enabled = false;
}

void APU::clockEnvelope() {
    auto env = [](uint8_t& vol, uint8_t period, uint8_t& timer, bool inc) {
        if (period == 0) return;
        if (--timer == 0) {
            timer = period;
            if (inc && vol < 15) vol++;
            else if (!inc && vol > 0) vol--;
        }
    };
    env(channel1.volume, channel1.envelopePeriod, channel1.envelopeTimer, channel1.envelopeIncrease);
    env(channel2.volume, channel2.envelopePeriod, channel2.envelopeTimer, channel2.envelopeIncrease);
    env(channel4.volume, channel4.envelopePeriod, channel4.envelopeTimer, channel4.envelopeIncrease);
}

void APU::generateSample() {
    int out[4] = {0, 0, 0, 0};
    bool active[4] = {false, false, false, false};

    if (channel1.enabled && channel1.dacEnabled){ 
        out[0] = DUTY[channel1.duty][channel1.dutyPosition] * channel1.volume; 
        active[0] = true;
    }
    if (channel2.enabled && channel2.dacEnabled) {
        out[1] = DUTY[channel2.duty][channel2.dutyPosition] * channel2.volume;
        active[1] = true;
    }
    if (channel3.enabled && channel3.dacEnabled) {
        uint8_t byte = mmu.memory[0xFF30 + channel3.position /2];
        uint8_t s = (channel3.position & 1) ? (byte & 0x0F) : (byte >> 4);
        static const int shift[4] = {4, 0, 1, 2};
        out[2] = s >> shift[channel3.volumeCode];
        active[2] = true;

    }
    if (channel4.enabled && channel4.dacEnabled) {
        out[3] = ((~channel4.lfsr) & 1) * channel4.volume; 
        active[3] = true;
    }

    float left = 0, right = 0;
    for (int c = 0; c < 4; c++) {
        if (!active[c]) continue;
        float v = (out[c] / 7.5f) - 1.0f;
        if (nr51 & (0x10 << c)) 
            left += v;
        if (nr51 & (0x01 << c)) 
            right += v;
    }
    left *= (((nr50 >> 4) & 7) + 1) /8.0f;
    right *= ((nr50 & 7) + 1) / 8.0f;

    left *=0.25f; right *=0.25f;

    sampleBuffer.push_back(static_cast<int16_t>(left * 12000));
    sampleBuffer.push_back(static_cast<int16_t>(right * 12000));
}

std::vector<int16_t> APU::getSamples() {
    std::vector<int16_t> out;
    out.swap(sampleBuffer);
    return out;
}

uint16_t APU::sweepCalculate() {
    uint16_t delta = channel1.sweepShadowFrequency >> channel1.sweepShift;
    uint16_t result = channel1.sweepNegate ? channel1.sweepShadowFrequency - delta : channel1.sweepShadowFrequency + delta;

    if (result > 2047) channel1.enabled = false;
    return result;
}
void APU::clockSweep() {
    if (channel1.sweepTimer > 0) channel1.sweepTimer--;

    if (channel1.sweepTimer == 0) {
        channel1.sweepTimer = channel1.sweepPeriod ? channel1.sweepPeriod : 8;

        if (channel1.sweepEnabled && channel1.sweepPeriod > 0) {
            uint16_t newFreq = sweepCalculate();

            if (newFreq <= 2047 && channel1.sweepShift > 0) {
                channel1.sweepShadowFrequency = newFreq;
                channel1.frequency = newFreq;
                sweepCalculate();
            }
        }
    }

}

void APU::writeRegister(uint16_t addr, uint8_t v) {
    // Wave RAM lives in the MMU's memory[]
    if (addr >= 0xFF30) { mmu.memory[addr] = v; return; }

    // While powered off, only NR52 is writable
    if (!enabled && addr != 0xFF26) return;

    regs[addr - 0xFF10] = v;

    auto writePulse = [&](PulseChannel& ch, int reg, uint8_t val) {
        switch (reg) {
            case 0: // NRx0: sweep (channel 1 only)
                ch.sweepPeriod = (val >> 4) & 7;
                ch.sweepNegate = val & 0x08;
                ch.sweepShift  = val & 7;
                break;
            case 1:
                ch.duty = val >> 6;
                ch.length = 64 - (val & 0x3F);
                break;
            case 2:
                ch.initialVolume = val >> 4;
                ch.envelopeIncrease = val & 0x08;
                ch.envelopePeriod = val & 7;
                ch.dacEnabled = (val & 0xF8) != 0;
                if (!ch.dacEnabled) ch.enabled = false;
                break;
            case 3:
                ch.frequency = (ch.frequency & 0x700) | val;
                break;
            case 4:
                ch.frequency = (ch.frequency & 0xFF) | ((val & 7) << 8);
                ch.lengthEnabled = val & 0x40;
                if (val & 0x80) {
                    ch.enabled = ch.dacEnabled;
                    if (ch.length == 0) ch.length = 64;
                    ch.timer = (2048 - ch.frequency) * 4;
                    ch.volume = ch.initialVolume;
                    ch.envelopeTimer = ch.envelopePeriod ? ch.envelopePeriod : 8;
                    ch.sweepShadowFrequency = ch.frequency;
                    ch.sweepTimer = ch.sweepPeriod ? ch.sweepPeriod : 8;
                    ch.sweepEnabled = ch.sweepPeriod || ch.sweepShift;
                    if (&ch == &channel1 && ch.sweepShift > 0)
                        sweepCalculate();
                }
                break;
        }
    };

    switch (addr) {
        case 0xFF10: writePulse(channel1, 0, v); break;
        case 0xFF11: writePulse(channel1, 1, v); break;
        case 0xFF12: writePulse(channel1, 2, v); break;
        case 0xFF13: writePulse(channel1, 3, v); break;
        case 0xFF14: writePulse(channel1, 4, v); break;

        case 0xFF16: writePulse(channel2, 1, v); break;
        case 0xFF17: writePulse(channel2, 2, v); break;
        case 0xFF18: writePulse(channel2, 3, v); break;
        case 0xFF19: writePulse(channel2, 4, v); break;
        case 0xFF1A:
            channel3.dacEnabled = v & 0x80;
            if (!channel3.dacEnabled) channel3.enabled = false;
            break;
        case 0xFF1B: channel3.length = 256 - v; break;
        case 0xFF1C: channel3.volumeCode = (v >> 5) & 3; break;
        case 0xFF1D: channel3.frequency = (channel3.frequency & 0x700) | v; break;
        case 0xFF1E:
            channel3.frequency = (channel3.frequency & 0xFF) | ((v & 7) << 8);
            channel3.lengthEnabled = v & 0x40;
            if (v & 0x80) {
                channel3.enabled = channel3.dacEnabled;
                if (channel3.length == 0) channel3.length = 256;
                channel3.timer = (2048 - channel3.frequency) * 2;
                channel3.position = 0;
            }
            break;

        case 0xFF20: channel4.length = 64 - (v & 0x3F); break;
        case 0xFF21:
            channel4.initialVolume = v >> 4;
            channel4.envelopeIncrease = v & 0x08;
            channel4.envelopePeriod = v & 7;
            channel4.dacEnabled = (v & 0xF8) != 0;
            if (!channel4.dacEnabled) channel4.enabled = false;
            break;
        case 0xFF22:
            channel4.clockShift  = v >> 4;
            channel4.narrowMode  = v & 0x08;
            channel4.divisorCode = v & 7;
            break;
        case 0xFF23:
            channel4.lengthEnabled = v & 0x40;
            if (v & 0x80) {
                static const int divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
                channel4.enabled = channel4.dacEnabled;
                if (channel4.length == 0) channel4.length = 64;
                channel4.timer = divisors[channel4.divisorCode] << channel4.clockShift;
                channel4.lfsr = 0x7FFF;
                channel4.volume = channel4.initialVolume;
                channel4.envelopeTimer = channel4.envelopePeriod ? channel4.envelopePeriod : 8;
            }
            break;
        case 0xFF24: nr50 = v; break;
        case 0xFF25: nr51 = v; break;
        case 0xFF26: {
            bool on = v & 0x80;
            if (!on && enabled) {
                for (auto& r : regs) r = 0;
                channel1 = PulseChannel{};
                channel2 = PulseChannel{};
                channel3 = WaveChannel{};
                channel4 = NoiseChannel{};
                nr50 = nr51 = 0;
            }
            if (on && !enabled) {
                frameSequencerCounter = 0;
                frameSequencerStep = 0;
            }
            enabled = on;
            break;
        }
    }
}

uint8_t APU::readRegister(uint16_t addr) {
    if (addr >= 0xFF30) return mmu.memory[addr];

    if (addr == 0xFF26) return 0x70 | (enabled ? 0x80 :0) | (channel1.enabled ? 1 : 0) |  (channel2.enabled ? 2 : 0) | (channel3.enabled ? 4 : 0) | (channel4.enabled ? 8 : 0);
    
    static const uint8_t mask[0x16] = {
        0x80, 0x3F, 0x00, 0xFF, 0xBF,
        0xFF, 0x3F, 0x00, 0xFF, 0xBF,
        0x7F, 0xFF, 0x9F, 0xFF, 0xBF,
        0xFF, 0xFF, 0x00, 0x00, 0xBF,
        0x00, 0x00
    };

    if (addr <= 0xFF25) return regs[addr - 0xFF10] | mask[addr - 0xFF10];
    return 0xFF;
}