#include "cpu.hpp"
#include <iostream>
#include <array>
#include <stdexcept>
#include <cstdint>

CPU::CPU(MMU& mmu) : mmu(mmu) {

    halted = false;
    PC = 0x0100;
    SP = 0xFFFE;
    ime = false;
    imeScheduled = false;

    A = 0x01; F = 0xB0;
    B = 0x00; C = 0x13;
    D = 0x00; E = 0xD8;
    H = 0x01; L = 0x4D;

    for (int i = 0; i < 256; i++) {
        opcodes[i] = &CPU::op_unknown;
    }

    for (int op = 0x40; op <=0x7F; op++) opcodes[op] = &CPU::op_ld_rr;
    opcodes[0x76] = &CPU::op_76;
    opcodes[0xCB]=&CPU::op_CB;
    for (int op = 0x80; op <= 0xBF; op++) opcodes[op] = &CPU::op_alu_r;
    for (int n = 0; n < 8; n++) {
        opcodes[0xc6 + n * 8] = &CPU::op_alu_n;
        opcodes[0x04 + n * 8] = &CPU::op_inc_r;
        opcodes[0x05 + n * 8] = &CPU::op_dec_r;
        opcodes[0x06 + n * 8] = &CPU::op_ld_r_n;
    }
    for (int op = 0xC7; op <= 0xFF; op += 8) opcodes[op] = &CPU::op_rst;

    opcodes[0x20] = &CPU::op_jr_cc;   opcodes[0x28] = &CPU::op_jr_cc;
    opcodes[0x30] = &CPU::op_jr_cc;   opcodes[0x38] = &CPU::op_jr_cc;
    opcodes[0xC2] = &CPU::op_jp_cc;   opcodes[0xCA] = &CPU::op_jp_cc;
    opcodes[0xD2] = &CPU::op_jp_cc;   opcodes[0xDA] = &CPU::op_jp_cc;
    opcodes[0xC4] = &CPU::op_call_cc; opcodes[0xCC] = &CPU::op_call_cc;
    opcodes[0xD4] = &CPU::op_call_cc; opcodes[0xDC] = &CPU::op_call_cc;
    opcodes[0xC0] = &CPU::op_ret_cc;  opcodes[0xC8] = &CPU::op_ret_cc;
    opcodes[0xD0] = &CPU::op_ret_cc;  opcodes[0xD8] = &CPU::op_ret_cc;

    opcodes[0xC1] = &CPU::op_pop;  opcodes[0xD1] = &CPU::op_pop;
    opcodes[0xE1] = &CPU::op_pop;  opcodes[0xF1] = &CPU::op_pop;
    opcodes[0xC5] = &CPU::op_push; opcodes[0xD5] = &CPU::op_push;
    opcodes[0xE5] = &CPU::op_push; opcodes[0xF5] = &CPU::op_push;

    opcodes[0x00] = &CPU::op_00; opcodes[0x01] = &CPU::op_01;
    opcodes[0x02] = &CPU::op_02; opcodes[0x03] = &CPU::op_03;
    opcodes[0x07] = &CPU::op_07; opcodes[0x08] = &CPU::op_08;
    opcodes[0x09] = &CPU::op_09; opcodes[0x0A] = &CPU::op_0A;
    opcodes[0x0B] = &CPU::op_0B; opcodes[0x0F] = &CPU::op_0F;
    opcodes[0x10] = &CPU::op_10; opcodes[0x11] = &CPU::op_11;
    opcodes[0x12] = &CPU::op_12; opcodes[0x13] = &CPU::op_13;
    opcodes[0x17] = &CPU::op_17; opcodes[0x18] = &CPU::op_18;
    opcodes[0x19] = &CPU::op_19; opcodes[0x1A] = &CPU::op_1A;
    opcodes[0x1B] = &CPU::op_1B; opcodes[0x1F] = &CPU::op_1F;
    opcodes[0x21] = &CPU::op_21; opcodes[0x22] = &CPU::op_22;
    opcodes[0x23] = &CPU::op_23; opcodes[0x27] = &CPU::op_27;
    opcodes[0x29] = &CPU::op_29; opcodes[0x2A] = &CPU::op_2A;
    opcodes[0x2B] = &CPU::op_2B; opcodes[0x2F] = &CPU::op_2F;
    opcodes[0x31] = &CPU::op_31; opcodes[0x32] = &CPU::op_32;
    opcodes[0x33] = &CPU::op_33; opcodes[0x37] = &CPU::op_37;
    opcodes[0x39] = &CPU::op_39; opcodes[0x3A] = &CPU::op_3A;
    opcodes[0x3B] = &CPU::op_3B; opcodes[0x3F] = &CPU::op_3F;
    opcodes[0xC3] = &CPU::op_C3; opcodes[0xC9] = &CPU::op_C9;
    opcodes[0xCB] = &CPU::op_CB; opcodes[0xCD] = &CPU::op_CD;
    opcodes[0xD9] = &CPU::op_D9;
    opcodes[0xE0] = &CPU::op_E0; opcodes[0xE2] = &CPU::op_E2;
    opcodes[0xE8] = &CPU::op_E8; opcodes[0xE9] = &CPU::op_E9;
    opcodes[0xEA] = &CPU::op_EA;
    opcodes[0xF0] = &CPU::op_F0; opcodes[0xF2] = &CPU::op_F2;
    opcodes[0xF3] = &CPU::op_F3; opcodes[0xF8] = &CPU::op_F8;
    opcodes[0xF9] = &CPU::op_F9; opcodes[0xFA] = &CPU::op_FA;
    opcodes[0xFB] = &CPU::op_FB;

}

static const uint8_t CYCLES[256] = {
    4, 12,  8,  8,  4,  4,  8,  4, 20,  8,  8,  8,  4,  4,  8,  4,
    4, 12,  8,  8,  4,  4,  8,  4, 12,  8,  8,  8,  4,  4,  8,  4,
    8, 12,  8,  8,  4,  4,  8,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    8, 12,  8,  8, 12, 12, 12,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    8,  8,  8,  8,  8,  8,  4,  8,  4,  4,  4,  4,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    8, 12, 12, 16, 12, 16,  8, 16,  8, 16, 12,  4, 12, 24,  8, 16,
    8, 12, 12,  4, 12, 16,  8, 16,  8, 16, 12,  4, 12,  4,  8, 16,
    12, 12,  8,  4,  4, 16,  8, 16, 16,  4, 16,  4,  4,  4,  8, 16,
    12, 12,  8,  4,  4, 16,  8, 16, 12,  8, 16,  4,  4,  4,  8, 16
};

void CPU::reset() {
    A = 0x01; F = 0xB0;
    B = 0x00; C = 0x13;
    D = 0x00; E = 0xD8;
    H = 0x01; L = 0x4D;
    SP = 0xFFFE;
    PC = 0x100;
    halted = false;
    ime = false;
    imeScheduled = false;

}

int CPU::step() {
    uint8_t pending = mmu.read(0xFFFF) & mmu.read(0xFF0F) & 0x1F;
    if (halted) {
        if(!pending) return 4;
        halted = false;
    }

    if (handleInterrupts()) {
        return 20;
    }
    
    bool enableIme = imeScheduled;
    extraCycles = 0;
    trace[traceIdx++ & 63] = { PC, mmu.read(PC), SP };

    
    uint8_t opcode = mmu.read(PC++);
    currentOp = opcode;
    (this->*opcodes[opcode])();

    if (enableIme && imeScheduled) {
        ime = true;
        imeScheduled = false;
    }

    return CYCLES[opcode] + extraCycles;
}

bool CPU::handleInterrupts() {
    uint8_t ie = mmu.read(0xFFFF);
    uint8_t if_reg = mmu.read(0xFF0F);
    uint8_t pending = ie & if_reg & 0x1F;
    
    if (!ime) return false;
    if(!pending) return false;

    for (int i = 0; i < 5; i++) {
        if (pending & (1 << i)) {
            ime = false;
            mmu.write(0xFF0F, mmu.read(0xFF0F) & ~(1 << i));
            mmu.write(--SP, PC >> 8);
            mmu.write(--SP, PC & 0xFF);
            PC = 0x40 + i * 8;
            return true;
        }
    }
    return false;
}

void CPU::op_unknown() {
    std::cerr << "Unknown opcode: 0x" << std::hex << (int)mmu.read(PC - 1) << std::endl;
    for (int i = 0; i< 64; i++) {
        auto& t = trace[(traceIdx + i) & 63];
        std::printf("PC=%04X OP=%02X SP=%04X\n", t.pc, t.op, t.sp);
    }
}

uint8_t CPU::fetch8() {
    return mmu.read(PC++);
}

uint16_t CPU::fetch16() {
    uint8_t low = mmu.read(PC++);
    uint8_t high = mmu.read(PC++);
    return (static_cast<uint16_t>(high) << 8) | low;
}

uint8_t CPU::readR(int i) {
    switch (i) {
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4: return H;
        case 5: return L;
        case 6: return mmu.read((H << 8) | L);
        default: return A;
    }
}

void CPU::writeR(int i, uint8_t v) {
    switch (i) {
        case 0: B = v; break;
        case 1: C = v; break;
        case 2: D = v; break;
        case 3: E = v; break;
        case 4: H = v; break;
        case 5: L = v; break;
        case 6: mmu.write((H << 8) | L, v); break;
        default: A = v; break;
    }
}

void CPU::executeCB(uint8_t op) {
    int r = op & 7;
    int n = (op >> 3) & 7;
    bool isHL = (r == 6);
    uint8_t v = readR(r);

    switch (op >> 6) {
        case 0: {
            uint8_t res = 0, c = 0;
            uint8_t oldC = (F >> 4) & 1;
            switch (n) {
                case 0: c = v >> 7; res = (v << 1) | c; break;
                case 1: c = v & 1; res = (v >> 1) | (c << 7); break;
                case 2: c = v >> 7; res = (v << 1) | oldC; break;
                case 3: c = v & 1; res = (v >> 1) | (oldC << 7); break;
                case 4: c = v >> 7; res = v << 1; break;                
                case 5: c = v & 1;  res = (v >> 1) | (v & 0x80); break; 
                case 6: c = 0;      res = (v << 4) | (v >> 4); break;   
                case 7: c = v & 1;  res = v >> 1; break;
            }
            F = (res == 0 ? 0x80 : 0) | (c ? 0x10 : 0);
            writeR(r, res);
            extraCycles = isHL ? 12 : 4;
            break;
        }
        case 1:
            F = (F & 0x10) | 0x20 | ((v & (1 << n)) ? 0: 0x80);
            extraCycles = isHL ? 8 : 4;
            break;
        case 2:
            writeR(r, v & ~(1 << n));
            extraCycles = isHL ? 12 : 4;
            break;
        case 3:
            writeR(r, v | (1 << n));
            extraCycles = isHL ? 12 : 4;
            break;
    }
}


void CPU::alu(int n, uint8_t v){
    uint8_t c = (F>> 4) & 1;
    int res;
    switch (n) {
        case 0:
            res = A + v;
            F = ((res & 0xFF) == 0 ? 0x80 : 0) | (((A & 0xF) + (v &0xF)) > 0xF ? 0x20 : 0) | (res > 0xFF ? 0x10 : 0);
            A = res & 0xFF;
            break;
        case 1:
            res = A + v + c;
            F = ((res & 0xFF) == 0 ? 0x80 : 0) | (((A & 0xF) + (v &0xF) + c) > 0xF ? 0x20 : 0) | (res > 0xFF ? 0x10 : 0);
            A = res & 0xFF;
            break;
        case 2:
            res = A - v;
            F = 0x40 | ((res & 0xFF) == 0 ? 0x80 : 0) | (((A & 0xF) < (v &0xF)) ? 0x20 : 0) | (A < v ? 0x10 : 0);
            A = res & 0xFF;
            break;
        case 3:
            res = A - v - c;
            F = 0x40 | ((res & 0xFF) == 0 ? 0x80 : 0) | (((A & 0xF) < (v &0xF) + c) ? 0x20 : 0) | (res < 0 ? 0x10 : 0);
            A = res & 0xFF;
            break;
        case 4: A &= v; F = (A == 0 ? 0x80 : 0) | 0x20; break;
        case 5: A ^= v; F = (A == 0 ? 0x80 : 0); break;
        case 6: A |= v; F = (A == 0 ? 0x80 : 0); break;
        case 7:
            res = A - v;
            F = 0x40 | ((res & 0xFF) == 0 ? 0x80 : 0) | ((A & 0xF) < (v &0xF) ? 0x20 : 0) | (A < v ? 0x10 : 0);
            break;
    }
}

void CPU::op_ld_rr(){ writeR((currentOp >> 3) & 7, readR(currentOp & 7)); }

void CPU::op_ld_r_n() { writeR((currentOp >> 3) & 7, fetch8()); }

void CPU::op_alu_r() { alu((currentOp >> 3) & 7, readR(currentOp & 7)); }
void CPU::op_alu_n() { alu((currentOp >> 3) & 7, fetch8()); }

void CPU::op_inc_r() {
    int r = (currentOp >> 3) & 7;
    uint8_t v = readR(r) + 1;
    F = (F & 0x10) | (v == 0 ? 0x80 : 0) | ((v & 0xF) == 0 ? 0x20 : 0);
    writeR(r, v);
}

void CPU::op_dec_r() {
    int r = (currentOp >> 3) & 7;
    uint8_t v = readR(r) - 1;
    F = (F & 0x10) | 0x40 | (v == 0 ? 0x80 : 0) | ((v & 0xF) == 0xF ? 0x20 : 0);
    writeR(r, v);
}

void CPU::addHL(uint16_t v) {
    uint16_t hl = getHL();
    uint32_t res = hl + v;
    F = (F & 0x80) | (((hl & 0xFFF) + (v & 0xFFF)) > 0xFFF ? 0x20 : 0) | (res > 0xFFFF ? 0x10 : 0);
    setHL(res & 0xFFFF);
}

void CPU::push16(uint16_t v) {
    mmu.write(--SP, v >> 8);
    mmu.write(--SP, v & 0xFF);
}

uint16_t CPU::pop16() {
    uint8_t lo = mmu.read(SP++);
    uint8_t hi = mmu.read(SP++);
    return (hi << 8) | lo;
}
bool CPU::condition(int cc) {
    switch (cc) {
        case 0: return !(F & 0x80);
        case 1: return (F & 0x80);
        case 2: return !(F & 0x10);
        default: return (F & 0x10);
    }
}

void CPU::op_push() {
    switch((currentOp >> 4 ) & 3) {
        case 0: push16((B << 8) | C); break;
        case 1: push16((D << 8) | E); break;
        case 2: push16((H << 8) | L); break;
        case 3: push16((A << 8) | F); break;
    }
}

void CPU::op_pop() {
    uint16_t v = pop16();
    switch((currentOp >> 4 ) & 3) {
        case 0: B = v >> 8; C = v & 0xFF; break;
        case 1: D = v >> 8; E = v & 0xFF; break;
        case 2: H = v >> 8; L = v & 0xFF; break;
        case 3: A = v >> 8; F = v & 0xF0; break;
    }
}

void CPU::op_jr_cc() {
    int8_t n = (int8_t)fetch8();
    if (condition((currentOp >> 3) & 3)) {
        PC += n;
        extraCycles = 4;
    }
}

void CPU::op_jp_cc() {
    uint16_t addr = fetch16();
    if (condition((currentOp >> 3) & 3)) {
        PC = addr;
        extraCycles = 4;
    }
}

void CPU::op_call_cc() {
    uint16_t addr = fetch16();
    if (condition((currentOp >> 3) & 3)) {
        push16(PC);
        PC = addr;
        extraCycles = 12;
    }
}

void CPU::op_ret_cc() {
    if (condition((currentOp >> 3) & 3)) {
        PC = pop16();
        extraCycles = 12;
    }
}

void CPU::op_rst() {
    push16(PC);
    PC = currentOp & 0x38;
}

void CPU::op_00() { }

void CPU::op_01() { C = fetch8(); B = fetch8(); }

void CPU::op_02() { mmu.write((B << 8) | C, A); }

void CPU::op_03() { uint16_t bc = (( B << 8) | C) + 1; B = bc >> 8; C = bc & 0xFF; }

void CPU::op_04() { B++; F = (F & 0x10) | (B == 0 ? 0x80 : 0) | ((B & 0xF) == 0xF ? 0x20 : 0);}

void CPU::op_05() { B--; F = (F & 0x10) | 0x40 | (B == 0 ? 0x80 : 0) | ((B & 0xF) == 0xF ? 0x20 : 0);}
 
void CPU::op_06() { B = fetch8(); }

void CPU::op_07() { uint8_t bit7 = A >> 7; A = (A << 1) | bit7; F = bit7 ? 0x10 : 0; }

void CPU::op_08() { 
    uint16_t addr = fetch16(); 
    mmu.write(addr, SP & 0xFF);
    mmu.write(addr + 1, SP >> 8);
}

void CPU::op_09() { addHL((B << 8) | C); }

void CPU::op_0A() { A = mmu.read((B << 8) | C); }

void CPU::op_0B() { uint16_t bc = ((B << 8) | C) - 1; B = bc >> 8; C = bc & 0xFF; }

void CPU::op_0C() { C++; F = (F & 0x10) | (C == 0 ? 0x80 : 0) | ((C & 0xF) == 0 ? 0x20 : 0); }

void CPU::op_0D() { C--; F = (F & 0x10) | 0x40 | (C == 0 ? 0x80 : 0) | ((C & 0xF) == 0xF ? 0x20 : 0); }

void CPU::op_0E() { C = fetch8(); }

void CPU::op_0F() {uint8_t bit0 = A & 1; A = (A >> 1) | (bit0 << 7); F = bit0 ? 0x10 : 0; }

void CPU::op_10() { fetch8(); halted = true; }

void CPU::op_11() { E = fetch8(); D = fetch8(); }

void CPU::op_12() { mmu.write((D << 8) | E, A); }

void CPU::op_13() { uint16_t de = ((D << 8) | E) + 1; D = de >> 8; E = de & 0xFF; }

void CPU::op_14() { D++; F = (F & 0x10) | (D == 0 ? 0x80 : 0) | ((D & 0xF) == 0 ? 0x20 : 0); }

void CPU::op_15() { D--; F = (F & 0x10) | 0x40 | (D == 0 ? 0x80 : 0) | ((D & 0xF) == 0xF ? 0x20 : 0); }

void CPU::op_16() { D = fetch8(); }

void CPU::op_17() { uint8_t carry = (F >> 4) & 1; F = (A >> 7) ? 0x10 : 0; A = (A << 1) | carry; }

void CPU::op_18() { int8_t n = (int8_t)fetch8(); PC+=n; }

void CPU::op_19() { addHL((D << 8) | E); }

void CPU::op_1A() {A = mmu.read((D << 8) | E); }

void CPU::op_1B() {uint16_t de = ((D << 8) | E) - 1; D = de >> 8; E = de & 0xFF;}

void CPU::op_1C() { E++; F = (F & 0x10) | (E == 0 ? 0x80 : 0) | ((E & 0xF) == 0 ? 0x20 : 0); }

void CPU::op_1D() { E--; F = (F & 0x10) | 0x40 | (E == 0 ? 0x80 : 0) | ((E & 0xF) == 0xF ? 0x20 : 0); }

void CPU::op_1E() { E = fetch8(); }

void CPU::op_1F() {uint8_t carry = (F >> 4) & 1; F = (A & 1) ? 0x10 : 0; A = (A >> 1) | (carry << 7);}


// 0x20–0x2F
void CPU::op_20() { int8_t n = (int8_t)fetch8(); if (!(F & 0x80)) PC +=n; }
void CPU::op_21() { L = fetch8(); H = fetch8();}
void CPU::op_22() { uint16_t hl = getHL(); mmu.write(hl, A); setHL(hl + 1); }
void CPU::op_23() { setHL(getHL() + 1); }
void CPU::op_24() { /* INC H - increment H, update flags */ }
void CPU::op_25() { /* DEC H - decrement H, update flags */ }
void CPU::op_26() { /* LD H,n - load immediate byte into H */ }
void CPU::op_27() { 
    int a = A;
    if (!(F & 0x40)) {
        if ((F & 0x10) || a > 0x99) { a += 0x60; F |= 0x10; }
        if ((F & 0x20) || (a & 0x0F) > 0x09) a += 0x06;
    }
    else {
        if (F & 0x10) a -= 0x60;
        if (F & 0x20) a -= 0x06;
    }
    A = a & 0xFF;
    F = (F & 0x10) | (A == 0 ? 0x80 : 0);
}
void CPU::op_28() { int8_t n = (int8_t)fetch8(); if (F & 0x80) PC +=n; }
void CPU::op_29() { addHL(getHL()); }
void CPU::op_2A() { uint16_t hl = getHL(); A = mmu.read(hl); setHL(hl + 1); }
void CPU::op_2B() { setHL(getHL() - 1); }
void CPU::op_2F() { A = ~A; F |= 0x60; }

void CPU::op_31() { SP = fetch16(); }
void CPU::op_32() { uint16_t hl = (H << 8) | L; mmu.write(hl, A); hl--; H = hl >> 8; L = hl & 0xFFl; }
void CPU::op_33() { SP++; }
void CPU::op_37() { F = (F & 0x80) | 0x10; }
void CPU::op_39() { addHL(SP); }
void CPU::op_3A() { uint16_t hl = getHL(); A = mmu.read(hl); setHL(hl - 1); }
void CPU::op_3B() { SP--; }
void CPU::op_3E() { A = fetch8(); }
void CPU::op_3F() { F = (F & 0x80) | ((F & 0x10) ? 0 : 0x10); }
void CPU::op_76() { halted = true; }
void CPU::op_AF() { A ^= A; F = 0x80; }
void CPU::op_C3() { PC = fetch16(); }
void CPU::op_C9() { uint8_t lo = mmu.read(SP++); uint8_t hi = mmu.read(SP++); PC = (hi << 8) | lo; }
void CPU::op_CB() { executeCB(fetch8()); }
void CPU::op_CD() { uint16_t nn = fetch16(); mmu.write(--SP, PC >> 8); mmu.write(--SP, PC & 0xFF); PC = nn; }
void CPU::op_D9() { PC = pop16(); ime = true; }

void CPU::op_E0() { mmu.write(0xFF00 + fetch8(), A); }
void CPU::op_E2() { mmu.write(0xFF00 + C, A); }
void CPU::op_E3() { ; }
void CPU::op_E4() { ; }
void CPU::op_E8() { 
    int8_t n = (int8_t)fetch8(); 
    uint16_t res = SP + n;
    F = (((SP & 0xF) + (n & 0xF)) > 0xF ? 0x20 : 0) | (((SP & 0xFF) + (n & 0xFF)) > 0xFF ? 0x10 : 0);
    SP = res;
}
void CPU::op_E9() { PC = (H << 8) | L; }
void CPU::op_EA() { mmu.write(fetch16(), A); }
void CPU::op_EB() { ; }
void CPU::op_EC() { ; }
void CPU::op_ED() { ; }

void CPU::op_F0() { A = mmu.read(0xFF00 + fetch8()); }
void CPU::op_F2() { A = mmu.read(0xFF00 + C); }
void CPU::op_F3() {ime = false; imeScheduled = false; }
void CPU::op_F4() { ; }
void CPU::op_F8() { 
    int8_t n = (int8_t)fetch8(); 
    uint16_t res = SP + n;
    F = (((SP & 0xF) + (n & 0xF)) > 0xF ? 0x20 : 0) | (((SP & 0xFF) + (n & 0xFF)) > 0xFF ? 0x10 : 0);
    setHL(res);
}
void CPU::op_F9() { SP = getHL(); }
void CPU::op_FA() { A = mmu.read(fetch16()); }
void CPU::op_FB() {  imeScheduled = true; }
void CPU::op_FC() { ; }
void CPU::op_FD() { ; }
void CPU::op_FE() { uint8_t n = fetch8(); uint8_t r = A - n; F = 0x40 | (r == 0 ? 0x80 : 0) | (A < n ? 0x10 : 0); }