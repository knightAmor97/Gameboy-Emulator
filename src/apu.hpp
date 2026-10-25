#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <iostream>
#include <SDL2/SDL.h>
#include "mmu.hpp"

class APU{
    public:
        explicit APU(MMU& mmu, int sampleRate = 44100);

        void step (int cycles);

        uint8_t readRegister(uint16_t addr);
        void writeRegister(uint16_t addr, uint8_t value);

        std::vector<int16_t> getSamples();

    private:
        MMU& mmu;

        bool enabled = false;
        int sampleRate = 44100;
        double cyclesPerSample = 4194304.0 / 44100.0;
        float hpLeft = 0, hpRight = 0;
        uint8_t nr50 = 0x77, nr51 = 0xFF;
        uint8_t regs[0x30] = {};

        struct PulseChannel {
            bool enabled = false;
            bool dacEnabled = false;

            uint16_t frequency = 0;
            int timer = 0;

            uint8_t duty = 0;
            uint8_t dutyPosition = 0;
            uint8_t initialVolume = 0;
            uint8_t volume = 0;
            uint8_t envelopePeriod = 0;
            uint8_t envelopeTimer = 0;
            bool envelopeIncrease = false;

            uint8_t length = 0;
            bool lengthEnabled = false;

            uint8_t sweepPeriod = 0;
            uint8_t sweepShift = 0;
            bool sweepNegate = false;
            bool sweepEnabled = false;
            uint16_t sweepShadowFrequency = 0;
            uint8_t sweepTimer = 0;
        };

        struct WaveChannel {
            bool enabled = false;
            bool dacEnabled = false;

            uint16_t frequency = 0;
            int timer = 0;
            uint8_t position = 0;
            uint8_t volumeCode = 0;
            uint16_t length = 0;
            bool lengthEnabled = false;

            std::array<uint8_t, 16> waveRAM{};
        };

        struct NoiseChannel {
            bool enabled = false;
            bool dacEnabled = false;

            int timer = 0;
            uint16_t lfsr = 0x7FFF;

            uint8_t initialVolume = 0;
            uint8_t volume = 0;
            uint8_t envelopePeriod = 0;
            uint8_t envelopeTimer = 0;
            bool envelopeIncrease = false;

            uint8_t clockShift = 0;
            uint8_t divisorCode = 0;
            bool narrowMode = false;
            
            uint8_t length = 0;
            bool lengthEnabled = false;

        };

        PulseChannel channel1;
        PulseChannel channel2;
        WaveChannel channel3;
        NoiseChannel channel4;

        uint16_t frameSequencerCounter = 0;
        uint8_t frameSequencerStep = 0;

        double sampleAccumulator = 0.0;
        std::vector<int16_t> sampleBuffer;

        void tickPulse(PulseChannel& channel);
        void tickWave();
        void tickNoise();

        void clockLength();
        void clockEnvelope();
        void clockSweep();
        uint16_t sweepCalculate();

        void generateSample();

};