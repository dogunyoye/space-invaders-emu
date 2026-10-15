#include <cstdio>
#include <cstdint>
#include <string>
#include "State8080.h"
#include "Emulator8080.h"
#include "ArcadeHardware.h"
#include "Disassembler8080.h"

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

int main() {
    test_shift_register();
    test_sound_latch();
    test_ports();
    test_disassembler();
    std::printf("hw: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
