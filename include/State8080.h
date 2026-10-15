#pragma once
#include <cstdint>
#include <vector>

struct ConditionCodes {
    uint8_t z:1;
    uint8_t s:1;
    uint8_t p:1;
    uint8_t cy:1;
    uint8_t ac:1;
};

struct State8080 {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    std::vector<uint8_t> memory;
    ConditionCodes cc;
    uint8_t int_enable;
    uint8_t halted;

    // --- Space Invaders Hardware / I/O State ---
    uint8_t port1 = 0;
    uint8_t port2 = 0;
    uint16_t shift_register = 0;
    uint8_t shift_offset = 0;

    State8080() : memory(65536, 0) {
        a = b = c = d = e = h = l = 0;
        sp = 0;
        pc = 0;
        int_enable = 0;
        halted = 0;
    }
};

inline uint16_t get_BC(const State8080 &s) { return (s.b << 8) | s.c; }
inline void set_BC(State8080 &s, uint16_t val) { s.b = (val >> 8) & 0xFF; s.c = val & 0xFF; }

inline uint16_t get_DE(const State8080 &s) { return (s.d << 8) | s.e; }
inline void set_DE(State8080 &s, uint16_t val) { s.d = (val >> 8) & 0xFF; s.e = val & 0xFF; }

inline uint16_t get_HL(const State8080 &s) { return (s.h << 8) | s.l; }
inline void set_HL(State8080 &s, uint16_t val) { s.h = (val >> 8) & 0xFF; s.l = val & 0xFF; }