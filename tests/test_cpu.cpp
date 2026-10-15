#include <cstdio>
#include <cstdint>
#include "State8080.h"
#include "Emulator8080.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void load(State8080 &s, uint16_t addr, std::initializer_list<uint8_t> bytes) {
    int i = 0;
    for (uint8_t b : bytes) s.memory[(uint16_t)(addr + i++)] = b;
}

static void test_mov_add_flags() {
    State8080 s;
    s.a = 0x0F;
    load(s, 0, {0x06, 0x01});  // MVI B,0x01
    CHECK(emulate8080(&s) == 7);
    CHECK(s.b == 0x01 && s.pc == 2);
    load(s, 2, {0x80});  // ADD B -> 0x10, AC=1
    s.pc = 2;
    CHECK(emulate8080(&s) == 4);
    CHECK(s.a == 0x10);
    CHECK(s.cc.ac == true);
    CHECK(s.cc.cy == false);
    CHECK(s.cc.z == false);
}

static void test_ana_ac() {
    State8080 s;
    // 8080 ANA: AC = (A|val) bit3
    s.a = 0x08;
    load(s, 0, {0xA0});  // ANA B (B=0x08)
    s.b = 0x08;
    emulate8080(&s);
    CHECK(s.a == 0x08);
    CHECK(s.cc.cy == false);
    CHECK(s.cc.ac == true);
    // XRA clears AC/CY
    load(s, 1, {0xA8});  // XRA B
    emulate8080(&s);
    CHECK(s.a == 0x00);
    CHECK(s.cc.z == true);
    CHECK(s.cc.ac == false);
}

static void test_dcr_inr_ac() {
    State8080 s;
    // DCR B: 0x01 -> 0x00 must set AC=1 (CPUTEST 0x11 case)
    s.b = 0x01;
    load(s, 0, {0x05});  // DCR B
    emulate8080(&s);
    CHECK(s.b == 0x00);
    CHECK(s.cc.z == true);
    CHECK(s.cc.ac == true);
    // DCR B: 0x10 -> 0x0F, low nibble was 0 so AC=0
    State8080 s2;
    s2.b = 0x10;
    load(s2, 0, {0x05});
    emulate8080(&s2);
    CHECK(s2.b == 0x0F);
    CHECK(s2.cc.ac == false);
    // INR B: 0x0F -> 0x10, AC=1
    State8080 s3;
    s3.b = 0x0F;
    load(s3, 0, {0x04});  // INR B
    emulate8080(&s3);
    CHECK(s3.b == 0x10);
    CHECK(s3.cc.ac == true);
}

static void test_dad_push_pop() {
    State8080 s;
    s.h = 0xFF; s.l = 0xFF; s.b = 0x00; s.c = 0x01;
    load(s, 0, {0x09});  // DAD B -> 0x0000, CY=1
    emulate8080(&s);
    CHECK(s.h == 0x00 && s.l == 0x00);
    CHECK(s.cc.cy == true);
    // PUSH B / POP D roundtrip
    State8080 s2;
    s2.sp = 0xF000; s2.b = 0x12; s2.c = 0x34;
    load(s2, 0, {0xC5, 0xD1});  // PUSH B, POP D
    emulate8080(&s2);
    CHECK(s2.sp == 0xEFFE);
    emulate8080(&s2);
    CHECK(s2.d == 0x12 && s2.e == 0x34);
    CHECK(s2.sp == 0xF000);
    // PSW roundtrip preserves flags
    State8080 s3;
    s3.sp = 0xF000; s3.a = 0xAB;
    s3.cc.z = true; s3.cc.cy = true;
    load(s3, 0, {0xF5, 0xF1});  // PUSH PSW, POP PSW
    s3.a = 0x00; s3.cc.z = false; s3.cc.cy = false;
    // reload: need fresh state for push values
    State8080 s4;
    s4.sp = 0xF000; s4.a = 0xAB;
    s4.cc.z = true; s4.cc.cy = true;
    load(s4, 0, {0xF5});
    emulate8080(&s4);
    s4.a = 0x00; s4.cc.z = false; s4.cc.cy = false;
    load(s4, 1, {0xF1});
    emulate8080(&s4);
    CHECK(s4.a == 0xAB);
    CHECK(s4.cc.z == true && s4.cc.cy == true);
}

static void test_jmp_call_ret() {
    State8080 s;
    load(s, 0, {0xC3, 0x05, 0x00});  // JMP 0x0005
    emulate8080(&s);
    CHECK(s.pc == 0x0005);
    // JNZ not taken when Z=1
    State8080 s2;
    s2.cc.z = true;
    load(s2, 0, {0xC2, 0x10, 0x00});  // JNZ 0x0010
    emulate8080(&s2);
    CHECK(s2.pc == 3);
    // CALL/RET
    State8080 s3;
    s3.sp = 0xF000;
    load(s3, 0, {0xCD, 0x05, 0x00, 0x00, 0x00, 0xC9});  // CALL 5; ... RET
    emulate8080(&s3);
    CHECK(s3.pc == 5 && s3.sp == 0xEFFE);
    emulate8080(&s3);
    CHECK(s3.pc == 3);
}

static void test_stack_wrap() {
    State8080 s;
    s.sp = 0x0000; s.b = 0xDE; s.c = 0xAD;
    load(s, 0, {0xC5});  // PUSH B at SP=0 wraps to 0xFFFE
    int cy = emulate8080(&s);
    CHECK(cy == 11);
    CHECK(s.sp == 0xFFFE);
    CHECK(s.memory[0xFFFF] == 0xDE && s.memory[0xFFFE] == 0xAD);
}

static void test_fetch_wrap() {
    // LXI at 0xFFFF must wrap second byte to 0x0000, not OOB.
    State8080 s;
    s.pc = 0xFFFF;
    s.memory[0xFFFF] = 0x01;  // LXI B
    s.memory[0x0000] = 0x34;
    s.memory[0x0001] = 0x12;
    CHECK(emulate8080(&s) == 10);
    CHECK(s.b == 0x12 && s.c == 0x34);
    CHECK(s.pc == 0x0002);
}

int main() {
    test_mov_add_flags();
    test_ana_ac();
    test_dcr_inr_ac();
    test_dad_push_pop();
    test_jmp_call_ret();
    test_stack_wrap();
    test_fetch_wrap();
    std::printf("cpu: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
