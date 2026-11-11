#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include "State8080.h"
#include "Emulator8080.h"
#include "ArcadeHardware.h"
#include "Disassembler8080.h"
#include "SaveState.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void test_shift_register() {
    State8080 s;
    arcade_sound_reset();
    // $0000, write $AA -> $AA00, write $FF -> $FFAA
    arcade_handle_out(s, 4, 0xAA);
    CHECK(s.shift_register == 0xAA00);
    arcade_handle_out(s, 4, 0xFF);
    CHECK(s.shift_register == 0xFFAA);
    // offset 0 -> high byte
    arcade_handle_out(s, 2, 0x00);
    CHECK(arcade_handle_in(s, 3) == 0xFF);
    // offset 4 -> middle nibble mix: 0xFFAA >> 4 = 0x0FFA -> 0xFA
    arcade_handle_out(s, 2, 0x04);
    CHECK(arcade_handle_in(s, 3) == 0xFA);
    // only low 3 bits of offset latch
    arcade_handle_out(s, 2, 0xFF);
    CHECK(s.shift_offset == 0x07);
}

static void test_sound_latch() {
    State8080 s;
    arcade_sound_reset();
    arcade_handle_out(s, 3, 0x01);
    CHECK(arcade_last_out3() == 0x01);
    arcade_ack_sound();
    CHECK(arcade_last_out3_prev() == 0x01);
    arcade_handle_out(s, 5, 0x10);
    CHECK(arcade_last_out5() == 0x10);
}

static void test_sound_edge_survives_rewrite() {
    // The ROM rewrites the latch several times per frame; a 0->1 edge must
    // still be visible at poll time if the same value is written twice.
    State8080 s;
    arcade_sound_reset();
    arcade_handle_out(s, 3, 0x02);
    arcade_handle_out(s, 3, 0x02);
    uint8_t edge3 = (uint8_t)((arcade_last_out3() ^ arcade_last_out3_prev()) & arcade_last_out3());
    CHECK(edge3 & 0x02);
    arcade_handle_out(s, 5, 0x01);
    arcade_handle_out(s, 5, 0x01);
    uint8_t edge5 = (uint8_t)((arcade_last_out5() ^ arcade_last_out5_prev()) & arcade_last_out5());
    CHECK(edge5 & 0x01);
    arcade_ack_sound();
    arcade_handle_out(s, 3, 0x02);
    edge3 = (uint8_t)((arcade_last_out3() ^ arcade_last_out3_prev()) & arcade_last_out3());
    CHECK(edge3 == 0);  // held level after ack is not a new edge
}

static void test_ports() {
    State8080 s;
    s.port1 = 0x01;  // coin
    s.port2 = 0x04;  // tilt
    CHECK(arcade_handle_in(s, 0) == 0x0E);
    // port1: bit3 forced high, bit7 masked off
    CHECK(arcade_handle_in(s, 1) == ((0x01 | 0x08) & 0x7F));
    CHECK(arcade_handle_in(s, 2) == 0x04);
    // OUT 6 (watchdog) is ignored, no crash
    arcade_handle_out(s, 6, 0xFF);
}

static void test_disassembler() {
    uint8_t code[3] = {0xC3, 0x34, 0x12};  // JMP $1234
    int len = 0;
    std::string d = disassemble8080_str(code, 0, &len);
    CHECK(len == 3);
    CHECK(d.find("JMP") != std::string::npos);
    CHECK(d.find("1234") != std::string::npos);
    uint8_t nop[1] = {0x00};
    d = disassemble8080_str(nop, 0, &len);
    CHECK(len == 1);
    CHECK(d.find("NOP") != std::string::npos);
}

static void test_disassembler_wraps_at_64k() {
    // 3-byte opcode at 0xFFFF: operands come from 0x0000/0x0001, no OOB read.
    State8080 s;
    s.memory[0xFFFF] = 0xC3;  // JMP
    s.memory[0x0000] = 0x34;
    s.memory[0x0001] = 0x12;
    int len = 0;
    std::string d = disassemble8080_str(s.memory.data(), 0xFFFF, &len);
    CHECK(len == 3);
    CHECK(d.find("1234") != std::string::npos);
}

static void test_save_load_roundtrip() {
    const std::string path =
        (std::filesystem::temp_directory_path() / "si_hw_test_save.bin").string();
    State8080 s;
    s.a = 0x11; s.b = 0x22; s.l = 0x77;
    s.sp = 0x23FE; s.pc = 0x1A2B;
    s.cc.z = true; s.cc.cy = true;
    s.int_enable = 1; s.halted = 1; s.ei_pending = true;
    s.shift_register = 0xBEEF; s.shift_offset = 5;
    s.memory[0x2400] = 0x5A; s.memory[0xFFFF] = 0xA5;
    arcade_sound_reset();
    arcade_handle_out(s, 3, 0x02);
    CHECK(save_state(s, path));

    State8080 r;
    r.port1 = 0x09;  // live inputs are kept, not overwritten
    arcade_sound_reset();
    CHECK(load_state(r, path));
    CHECK(r.a == 0x11 && r.b == 0x22 && r.l == 0x77);
    CHECK(r.sp == 0x23FE && r.pc == 0x1A2B);
    CHECK(r.cc.z && r.cc.cy && !r.cc.s);
    CHECK(r.int_enable == 1 && r.halted == 1 && r.ei_pending);
    CHECK(r.shift_register == 0xBEEF && r.shift_offset == 5);
    CHECK(r.memory[0x2400] == 0x5A && r.memory[0xFFFF] == 0xA5);
    CHECK(r.port1 == 0x09);
    CHECK(arcade_last_out3() == 0x02);
    std::filesystem::remove(path);
}

static void test_load_rejects_bad_file_without_mutating() {
    const std::string path =
        (std::filesystem::temp_directory_path() / "si_hw_test_bad.bin").string();
    State8080 good;
    good.a = 0x42;
    CHECK(save_state(good, path));
    // Truncate: valid header, missing most of memory.
    std::filesystem::resize_file(path, 100);
    State8080 live;
    live.a = 0x99; live.pc = 0x1234; live.memory[0x2000] = 0x77;
    CHECK(!load_state(live, path));
    CHECK(live.a == 0x99 && live.pc == 0x1234 && live.memory[0x2000] == 0x77);
    // Wrong magic, right size.
    { std::ofstream f(path, std::ios::binary | std::ios::trunc);
      std::string junk(65536 + 64, 'x'); f << junk; }
    CHECK(!load_state(live, path));
    CHECK(live.a == 0x99);
    CHECK(!load_state(live, path + ".missing"));
    std::filesystem::remove(path);
}

int main() {
    test_shift_register();
    test_sound_latch();
    test_sound_edge_survives_rewrite();
    test_ports();
    test_disassembler();
    test_disassembler_wraps_at_64k();
    test_save_load_roundtrip();
    test_load_rejects_bad_file_without_mutating();
    std::printf("hw: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
