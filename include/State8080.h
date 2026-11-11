#pragma once
#include <array>
#include <cstdint>

struct ConditionCodes {
    bool z = false;
    bool s = false;
    bool p = false;
    bool cy = false;
    bool ac = false;
};

struct State8080 {
    uint8_t a = 0;
    uint8_t b = 0;
    uint8_t c = 0;
    uint8_t d = 0;
    uint8_t e = 0;
    uint8_t h = 0;
    uint8_t l = 0;
    uint16_t sp = 0;
    uint16_t pc = 0;
    std::array<uint8_t, 65536> memory{};
    ConditionCodes cc{};
    uint8_t int_enable = 0;
    uint8_t halted = 0;
    // EI takes effect after the following instruction (8080 behaviour).
    bool ei_pending = false;

    // Memory map. Defaults give flat 64K RAM (CP/M diagnostics). Space
    // Invaders decodes 14 address bits (RAM mirrors above 0x4000) and has
    // ROM at 0x0000-0x1FFF: set addr_mask = 0x3FFF, rom_end = 0x2000.
    uint16_t addr_mask = 0xFFFF;
    uint16_t rom_end = 0;  // CPU writes below this address are ignored

    // Error latch: set when emulate8080 hits an unimplemented opcode.
    // emulate8080 returns 0 in that case (no exit()).
    bool emulation_error = false;
    uint8_t error_opcode = 0;
    uint16_t error_pc = 0;

    // --- Space Invaders Hardware / I/O State ---
    uint8_t port1 = 0;
    uint8_t port2 = 0;
    uint16_t shift_register = 0;
    uint8_t shift_offset = 0;

    State8080() { memory.fill(0); }
};

inline uint16_t get_BC(const State8080 &s) {
    return (uint16_t)((s.b << 8) | s.c);
}
inline void set_BC(State8080 &s, uint16_t val) {
    s.b = (uint8_t)((val >> 8) & 0xFF);
    s.c = (uint8_t)(val & 0xFF);
}

inline uint16_t get_DE(const State8080 &s) {
    return (uint16_t)((s.d << 8) | s.e);
}
inline void set_DE(State8080 &s, uint16_t val) {
    s.d = (uint8_t)((val >> 8) & 0xFF);
    s.e = (uint8_t)(val & 0xFF);
}

inline uint16_t get_HL(const State8080 &s) {
    return (uint16_t)((s.h << 8) | s.l);
}
inline void set_HL(State8080 &s, uint16_t val) {
    s.h = (uint8_t)((val >> 8) & 0xFF);
    s.l = (uint8_t)(val & 0xFF);
}
