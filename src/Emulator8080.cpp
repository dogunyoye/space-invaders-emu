#include "State8080.h"
#include "Emulator8080.h"
#include "ArcadeHardware.h"
#include <iostream>
#include <cstdlib>

// ---------- Flag helpers ----------

// Even parity: returns 1 if even number of bits set
static uint8_t parity_even(uint8_t v) {
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return (~v) & 1;
}

void set_zsp_flags(State8080 &state, uint8_t value) {
    state.cc.z = (value == 0);
    state.cc.s = ((value & 0x80) != 0);
    state.cc.p = parity_even(value);
}

// Provided for compatibility with the forward declaration (unused elsewhere)
uint8_t add_bytes(State8080 &state, uint8_t a, uint8_t b, uint8_t carry) {
    uint16_t result = (uint16_t)a + (uint16_t)b + (uint16_t)carry;
    state.cc.cy = (result > 0xFF);
    state.cc.ac = (((a & 0x0F) + (b & 0x0F) + carry) > 0x0F);
    uint8_t truncated = (uint8_t)result;
    set_zsp_flags(state, truncated);
    return truncated;
}

// Helper to get value from a register code (0=B, 1=C, 2=D, 3=E, 4=H, 5=L, 6=M(hl), 7=A)
uint8_t get_reg_val(const State8080 &state, uint8_t code) {
    uint16_t hl = (state.h << 8) | state.l;
    switch (code) {
        case 0: return state.b;
        case 1: return state.c;
        case 2: return state.d;
        case 3: return state.e;
        case 4: return state.h;
        case 5: return state.l;
        case 6: return state.memory[hl]; // Memory reference via HL
        case 7: return state.a;
        default: return 0;
    }
}

// Helper to set value into a register code
void set_reg_val(State8080 &state, uint8_t code, uint8_t val) {
    uint16_t hl = (state.h << 8) | state.l;
    switch (code) {
        case 0: state.b = val; break;
        case 1: state.c = val; break;
        case 2: state.d = val; break;
        case 3: state.e = val; break;
        case 4: state.h = val; break;
        case 5: state.l = val; break;
        case 6: state.memory[hl] = val; break; // Memory write via HL
        case 7: state.a = val; break;
    }
}

// Helper for addition that updates all flags (Z, S, P, CY, AC)
uint8_t add_to_accumulator(State8080 &state, uint8_t val) {
    uint16_t result = (uint16_t)state.a + (uint16_t)val;

    // Carry flag: set if result exceeds 255 (bit 8 is set)
    state.cc.cy = (result > 0xFF);

    // Auxiliary Carry: carry from bit 3 to bit 4
    state.cc.ac = (((state.a & 0x0F) + (val & 0x0F)) > 0x0F);

    uint8_t truncated = (uint8_t)result;
    set_zsp_flags(state, truncated);

    state.a = truncated;
    return truncated;
}

// Helper for addition with carry that updates all flags (Z, S, P, CY, AC)
uint8_t add_with_carry_to_accumulator(State8080 &state, uint8_t val) {
    uint8_t old_cy = state.cc.cy;
    uint16_t result = (uint16_t)state.a + (uint16_t)val + (uint16_t)old_cy;

    // Carry flag: set if result exceeds 255
    state.cc.cy = (result > 0xFF);

    // Auxiliary Carry includes the previous carry
    state.cc.ac = (((state.a & 0x0F) + (val & 0x0F) + old_cy) > 0x0F);

    uint8_t truncated = (uint8_t)result;
    set_zsp_flags(state, truncated);

    state.a = truncated;
    return truncated;
}

// Helper for subtraction that updates all flags (Z, S, P, CY, AC)
// CY = borrow (1 if a < val). AC on 8080 = NOT half-borrow
// (carry out of bit 3 of a + ~val + 1), i.e. 1 when low nibble does NOT borrow.
uint8_t subtract_from_accumulator(State8080 &state, uint8_t val) {
    // Carry flag acts as borrow: set if value to subtract is greater than accumulator
    state.cc.cy = (state.a < val);

    // Auxiliary Carry (8080): set if NO borrow from bit 4
    state.cc.ac = ((state.a & 0x0F) >= (val & 0x0F));

    uint16_t result = (uint16_t)state.a - (uint16_t)val;
    uint8_t truncated = (uint8_t)result;
    set_zsp_flags(state, truncated);

    state.a = truncated;
    return truncated;
}

// Helper for subtraction with borrow that updates all flags (Z, S, P, CY, AC)
uint8_t subtract_with_borrow_from_accumulator(State8080 &state, uint8_t val) {
    uint8_t old_cy = state.cc.cy;
    uint16_t subtrahend = (uint16_t)val + (uint16_t)old_cy;
    state.cc.cy = ((uint16_t)state.a < subtrahend);

    // Auxiliary Carry (8080): NOT half-borrow including incoming borrow
    state.cc.ac = ((state.a & 0x0F) >= ((val & 0x0F) + old_cy));

    uint16_t result = (uint16_t)state.a - subtrahend;
    uint8_t truncated = (uint8_t)result;
    set_zsp_flags(state, truncated);

    state.a = truncated;
    return truncated;
}

// ---------- Logical / compare helpers ----------

static void ana_accumulator(State8080 &state, uint8_t val) {
    // 8080: ANA sets CY=0, AC = (A | val) bit3 (differs from 8085 which always sets AC=1).
    // Reference: superzazu/8080 (passes CPUTEST.COM), Intel 8080 manual.
    uint8_t orig_a = state.a;
    state.a = state.a & val;
    state.cc.cy = 0;
    state.cc.ac = ((orig_a | val) & 0x08) != 0;
    set_zsp_flags(state, state.a);
}

static void xra_accumulator(State8080 &state, uint8_t val) {
    // 8080: XRA sets CY=0, AC=0
    state.a = state.a ^ val;
    state.cc.cy = 0;
    state.cc.ac = 0;
    set_zsp_flags(state, state.a);
}

static void ora_accumulator(State8080 &state, uint8_t val) {
    // 8080: ORA sets CY=0, AC=0
    state.a = state.a | val;
    state.cc.cy = 0;
    state.cc.ac = 0;
    set_zsp_flags(state, state.a);
}

static void cmp_accumulator(State8080 &state, uint8_t val) {
    // Compare: like SUB but result discarded; flags set accordingly.
    // CY = borrow (a < val). AC = NOT low-nibble borrow (8080).
    uint16_t result = (uint16_t)state.a - (uint16_t)val;
    state.cc.cy = ((uint16_t)state.a < (uint16_t)val);
    state.cc.ac = ((state.a & 0x0F) >= (val & 0x0F));
    set_zsp_flags(state, (uint8_t)result);
}

// INR: increment register/memory, affects Z,S,P,AC (not CY)
static uint8_t inr_value(State8080 &state, uint8_t val) {
    uint8_t result = val + 1;
    // AC set on carry out of bit 3
    state.cc.ac = ((val & 0x0F) == 0x0F);
    set_zsp_flags(state, result);
    return result;
}

// DCR: decrement register/memory, affects Z,S,P,AC (not CY)
static uint8_t dcr_value(State8080 &state, uint8_t val) {
    uint8_t result = val - 1;
    // 8080 AC for DCR: set when no borrow from bit 4 (i.e. low nibble != 0).
    // Equivalent to !((result & 0x0F) == 0x0F). Verified against CPUTEST.COM
    // test 0x11 (DCR B): 0x01 -> 0x00 must set AC=1.
    state.cc.ac = ((val & 0x0F) != 0x00);
    set_zsp_flags(state, result);
    return result;
}

// ---------- Stack helpers ----------

static void push_word(State8080 *state, uint8_t high, uint8_t low) {
    state->memory[state->sp - 1] = high;
    state->memory[state->sp - 2] = low;
    state->sp -= 2;
}

static void pop_word(State8080 *state, uint8_t &high, uint8_t &low) {
    low = state->memory[state->sp];
    high = state->memory[state->sp + 1];
    state->sp += 2;
}

static uint8_t flags_to_psw(const State8080 &state) {
    return (state.cc.s << 7) | (state.cc.z << 6) |
           (0 << 5) | (state.cc.ac << 4) |
           (0 << 3) | (state.cc.p << 2) |
           (1 << 1) | (state.cc.cy);
}

static void psw_to_flags(State8080 &state, uint8_t psw) {
    state.cc.s  = (psw >> 7) & 1;
    state.cc.z  = (psw >> 6) & 1;
    state.cc.ac = (psw >> 4) & 1;
    state.cc.p  = (psw >> 2) & 1;
    state.cc.cy = (psw >> 0) & 1;
}

// Condition field ccc = (opcode >> 3) & 7 : 0=NZ 1=Z 2=NC 3=C 4=PO 5=PE 6=P 7=M
static bool check_condition(const State8080 &state, uint8_t ccc) {
    switch (ccc) {
        case 0: return state.cc.z == 0; // NZ
        case 1: return state.cc.z == 1; // Z
        case 2: return state.cc.cy == 0; // NC
        case 3: return state.cc.cy == 1; // C
        case 4: return state.cc.p == 0; // PO
        case 5: return state.cc.p == 1; // PE
        case 6: return state.cc.s == 0; // P
        case 7: return state.cc.s == 1; // M
        default: return false;
    }
}

void emulate_interrupt(State8080 *state, int interrupt_num) {
    if (!state->int_enable) return;

    state->int_enable = 0; // Disable further interrupts until enabled via EI

    // A real 8080 leaves HLT when an enabled interrupt arrives.
    state->halted = 0;

    // Push PC
    push_word(state, (state->pc >> 8) & 0xFF, state->pc & 0xFF);
    
    // Set PC to RST vector (RST 1 = 0x08, RST 2 = 0x10, etc.)
    // interrupt_num 1 -> 0x0008, 2 -> 0x0010
    state->pc = interrupt_num * 8;
}

int emulate8080(State8080 *state) {
    // Fetch opcode
    uint8_t opcode = state->memory[state->pc];

    // Advance PC past the opcode (instructions can advance PC further if they take arguments)
    state->pc++;

    // Unofficial or base cycle count (optional for now, but good to structure)
    int cycles = 4;

    // Handle MOV instruction range: 0x40 to 0x7F (excluding HLT at 0x76)
    if (opcode >= 0x40 && opcode <= 0x7F && opcode != 0x76) {
        uint8_t dest = (opcode >> 3) & 7;
        uint8_t src = opcode & 7;
        uint8_t val = get_reg_val(*state, src);
        set_reg_val(*state, dest, val);

        // MOV to/from M takes slightly more cycles
        if (dest == 6 || src == 6) cycles = 7;
        else cycles = 5;
        return cycles;
    }

    // Handle ADD instruction range: 0x80 to 0x87
    if (opcode >= 0x80 && opcode <= 0x87) {
        uint8_t src = opcode & 7;
        uint8_t val = get_reg_val(*state, src);
        add_to_accumulator(*state, val);

        // ADD M takes 7 cycles, register adds take 4 cycles
        return (src == 6) ? 7 : 4;
    }

    // Handle ADC instruction range: 0x88 to 0x8F
    if (opcode >= 0x88 && opcode <= 0x8F) {
        uint8_t src = opcode & 7;
        uint8_t val = get_reg_val(*state, src);
        add_with_carry_to_accumulator(*state, val);

        // ADC M takes 7 cycles, register ADCs take 4 cycles
        return (src == 6) ? 7 : 4;
    }

    // Handle SUB instruction range: 0x90 to 0x97
    if (opcode >= 0x90 && opcode <= 0x97) {
        uint8_t src = opcode & 7;
        uint8_t val = get_reg_val(*state, src);
        subtract_from_accumulator(*state, val);

        // SUB M takes 7 cycles, register SUBs take 4 cycles
        return (src == 6) ? 7 : 4;
    }

    // Handle SBB instruction range: 0x98 to 0x9F
    if (opcode >= 0x98 && opcode <= 0x9F) {
        uint8_t src = opcode & 7;
        uint8_t val = get_reg_val(*state, src);
        subtract_with_borrow_from_accumulator(*state, val);

        // SBB M takes 7 cycles, register SBBs take 4 cycles
        return (src == 6) ? 7 : 4;
    }

    // Handle ANA instruction range: 0xA0 to 0xA7
    if (opcode >= 0xA0 && opcode <= 0xA7) {
        uint8_t src = opcode & 7;
        ana_accumulator(*state, get_reg_val(*state, src));
        return (src == 6) ? 7 : 4;
    }

    // Handle XRA instruction range: 0xA8 to 0xAF
    if (opcode >= 0xA8 && opcode <= 0xAF) {
        uint8_t src = opcode & 7;
        xra_accumulator(*state, get_reg_val(*state, src));
        return (src == 6) ? 7 : 4;
    }

    // Handle ORA instruction range: 0xB0 to 0xB7
    if (opcode >= 0xB0 && opcode <= 0xB7) {
        uint8_t src = opcode & 7;
        ora_accumulator(*state, get_reg_val(*state, src));
        return (src == 6) ? 7 : 4;
    }

    // Handle CMP instruction range: 0xB8 to 0xBF
    if (opcode >= 0xB8 && opcode <= 0xBF) {
        uint8_t src = opcode & 7;
        cmp_accumulator(*state, get_reg_val(*state, src));
        return (src == 6) ? 7 : 4;
    }

    // Handle INR instruction pattern: 00DDD100 (0x04,0x0C,0x14,0x1C,0x24,0x2C,0x34,0x3C)
    if ((opcode & 0xC7) == 0x04) {
        uint8_t reg = (opcode >> 3) & 7;
        uint8_t val = get_reg_val(*state, reg);
        uint8_t res = inr_value(*state, val);
        set_reg_val(*state, reg, res);
        return (reg == 6) ? 10 : 5;
    }

    // Handle DCR instruction pattern: 00DDD101 (0x05,0x0D,0x15,0x1D,0x25,0x2D,0x35,0x3D)
    if ((opcode & 0xC7) == 0x05) {
        uint8_t reg = (opcode >> 3) & 7;
        uint8_t val = get_reg_val(*state, reg);
        uint8_t res = dcr_value(*state, val);
        set_reg_val(*state, reg, res);
        return (reg == 6) ? 10 : 5;
    }

    // Handle MVI pattern: 00DDD110 (covers 0x06,0x0E,0x16,0x1E,0x26,0x2E,0x36,0x3E)
    if ((opcode & 0xC7) == 0x06) {
        uint8_t reg = (opcode >> 3) & 7;
        uint8_t val = state->memory[state->pc];
        state->pc++;
        set_reg_val(*state, reg, val);
        return (reg == 6) ? 10 : 7;
    }

    switch (opcode) {
        case 0x00: // NOP
        case 0x08: // NOP (undocumented)
        case 0x10: // NOP (undocumented)
        case 0x18: // NOP (undocumented)
        case 0x20: // NOP (RIM on 8085)
        case 0x28: // NOP (undocumented)
        case 0x30: // NOP (SIM on 8085)
        case 0x38: // NOP (undocumented)
        case 0xCB: // NOP (undocumented)
        case 0xD9: // NOP (undocumented)
        case 0xDD: // NOP (undocumented)
        case 0xED: // NOP (undocumented)
        case 0xFD: // NOP (undocumented)
            cycles = 4;
            break;

        case 0x01: // LXI B, d16
            state->c = state->memory[state->pc];
            state->b = state->memory[state->pc + 1];
            state->pc += 2;
            cycles = 10;
            break;

        case 0x11: // LXI D, d16
            state->e = state->memory[state->pc];
            state->d = state->memory[state->pc + 1];
            state->pc += 2;
            cycles = 10;
            break;

        case 0x21: // LXI H, d16
            state->l = state->memory[state->pc];
            state->h = state->memory[state->pc + 1];
            state->pc += 2;
            cycles = 10;
            break;

        case 0x31: // LXI SP, d16
            state->sp = (state->memory[state->pc + 1] << 8) | state->memory[state->pc];
            state->pc += 2;
            cycles = 10;
            break;

        case 0x02: { // STAX B
            uint16_t addr = get_BC(*state);
            state->memory[addr] = state->a;
            cycles = 7;
            break;
        }

        case 0x12: { // STAX D
            uint16_t addr = get_DE(*state);
            state->memory[addr] = state->a;
            cycles = 7;
            break;
        }

        case 0x0A: { // LDAX B
            uint16_t addr = get_BC(*state);
            state->a = state->memory[addr];
            cycles = 7;
            break;
        }

        case 0x1A: { // LDAX D
            uint16_t addr = get_DE(*state);
            state->a = state->memory[addr];
            cycles = 7;
            break;
        }

        case 0x03: { // INX B
            uint16_t bc = get_BC(*state) + 1;
            set_BC(*state, bc);
            cycles = 5;
            break;
        }

        case 0x13: { // INX D
            uint16_t de = get_DE(*state) + 1;
            set_DE(*state, de);
            cycles = 5;
            break;
        }

        case 0x23: { // INX H
            uint16_t hl = get_HL(*state) + 1;
            set_HL(*state, hl);
            cycles = 5;
            break;
        }

        case 0x33: // INX SP
            state->sp += 1;
            cycles = 5;
            break;

        case 0x0B: { // DCX B
            uint16_t bc = get_BC(*state) - 1;
            set_BC(*state, bc);
            cycles = 5;
            break;
        }

        case 0x1B: { // DCX D
            uint16_t de = get_DE(*state) - 1;
            set_DE(*state, de);
            cycles = 5;
            break;
        }

        case 0x2B: { // DCX H
            uint16_t hl = get_HL(*state) - 1;
            set_HL(*state, hl);
            cycles = 5;
            break;
        }

        case 0x3B: // DCX SP
            state->sp -= 1;
            cycles = 5;
            break;

        case 0x09: { // DAD B
            uint32_t hl = get_HL(*state);
            uint32_t bc = get_BC(*state);
            uint32_t res = hl + bc;
            state->cc.cy = (res > 0xFFFF);
            set_HL(*state, (uint16_t)res);
            cycles = 10;
            break;
        }

        case 0x19: { // DAD D
            uint32_t hl = get_HL(*state);
            uint32_t de = get_DE(*state);
            uint32_t res = hl + de;
            state->cc.cy = (res > 0xFFFF);
            set_HL(*state, (uint16_t)res);
            cycles = 10;
            break;
        }

        case 0x29: { // DAD H
            uint32_t hl = get_HL(*state);
            uint32_t res = hl + hl;
            state->cc.cy = (res > 0xFFFF);
            set_HL(*state, (uint16_t)res);
            cycles = 10;
            break;
        }

        case 0x39: { // DAD SP
            uint32_t hl = get_HL(*state);
            uint32_t res = hl + state->sp;
            state->cc.cy = (res > 0xFFFF);
            set_HL(*state, (uint16_t)res);
            cycles = 10;
            break;
        }

        case 0x07: { // RLC
            uint8_t carry = (state->a >> 7) & 1;
            state->a = (state->a << 1) | carry;
            state->cc.cy = carry;
            cycles = 4;
            break;
        }

        case 0x0F: { // RRC
            uint8_t carry = state->a & 1;
            state->a = (state->a >> 1) | (carry << 7);
            state->cc.cy = carry;
            cycles = 4;
            break;
        }

        case 0x17: { // RAL
            uint8_t carry = (state->a >> 7) & 1;
            state->a = (state->a << 1) | state->cc.cy;
            state->cc.cy = carry;
            cycles = 4;
            break;
        }

        case 0x1F: { // RAR
            uint8_t carry = state->a & 1;
            state->a = (state->a >> 1) | (state->cc.cy << 7);
            state->cc.cy = carry;
            cycles = 4;
            break;
        }

        case 0x22: { // SHLD adr
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            state->memory[addr] = state->l;
            state->memory[addr + 1] = state->h;
            cycles = 16;
            break;
        }

        case 0x2A: { // LHLD adr
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            state->l = state->memory[addr];
            state->h = state->memory[addr + 1];
            cycles = 16;
            break;
        }

        case 0x32: { // STA adr
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            state->memory[addr] = state->a;
            cycles = 13;
            break;
        }

        case 0x3A: { // LDA adr
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            state->a = state->memory[addr];
            cycles = 13;
            break;
        }

        case 0x27: { // DAA
            // Reference behavior (passes CPUTEST.COM test 0x67):
            // correction based on original A nibbles + old AC/CY,
            // then ADD correction (which sets ZSP+AC correctly), CY forced.
            uint8_t a = state->a;
            bool old_cy = state->cc.cy;
            bool old_ac = state->cc.ac;
            uint8_t lo = a & 0x0F;
            uint8_t hi = a >> 4;
            uint8_t correction = 0;
            if (old_ac || lo > 9)
                correction += 0x06;
            bool cy = old_cy;
            if (old_cy || hi > 9 || (hi >= 9 && lo > 9)) {
                correction += 0x60;
                cy = 1;
            }
            add_to_accumulator(*state, correction);
            state->cc.cy = cy;
            cycles = 4;
            break;
        }

        case 0x2F: // CMA
            state->a = ~state->a;
            cycles = 4;
            break;

        case 0x37: // STC
            state->cc.cy = 1;
            cycles = 4;
            break;

        case 0x3F: // CMC
            state->cc.cy = ~state->cc.cy & 1;
            cycles = 4;
            break;

        case 0x76: // HLT
            state->halted = 1;
            state->pc--; // stay on HLT so interrupt can resume (conventional)
            cycles = 7;
            break;

        case 0xC6: // ADI d8
            add_to_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xCE: // ACI d8
            add_with_carry_to_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xD6: // SUI d8
            subtract_from_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xDE: // SBI d8
            subtract_with_borrow_from_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xE6: // ANI d8
            ana_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xEE: // XRI d8
            xra_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xF6: // ORI d8
            ora_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xFE: // CPI d8
            cmp_accumulator(*state, state->memory[state->pc]);
            state->pc++;
            cycles = 7;
            break;

        case 0xC3: // JMP adr
            state->pc = (state->memory[state->pc + 1] << 8) | state->memory[state->pc];
            cycles = 10;
            break;

        case 0xC2: // JNZ adr
        case 0xCA: // JZ adr
        case 0xD2: // JNC adr
        case 0xDA: // JC adr
        case 0xE2: // JPO adr
        case 0xEA: // JPE adr
        case 0xF2: // JP adr
        case 0xFA: // JM adr
        {
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            uint8_t ccc = (opcode >> 3) & 7;
            // Jcc mapping: JNZ=000, JZ=001, JNC=010, JC=011, JPO=100, JPE=101, JP=110, JM=111
            // (opcode>>3)&7 yields 0..7 in order for C2,CA,D2,DA,E2,EA,F2,FA. Verified.
            if (check_condition(*state, ccc))
                state->pc = addr;
            cycles = 10;
            break;
        }

        case 0xCD: { // CALL adr
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            push_word(state, (state->pc >> 8) & 0xFF, state->pc & 0xFF);
            state->pc = addr;
            cycles = 17;
            break;
        }

        case 0xC4: // CNZ adr
        case 0xCC: // CZ adr
        case 0xD4: // CNC adr
        case 0xDC: // CC adr
        case 0xE4: // CPO adr
        case 0xEC: // CPE adr
        case 0xF4: // CP adr
        case 0xFC: // CM adr
        {
            uint16_t addr = state->memory[state->pc] | (state->memory[state->pc + 1] << 8);
            state->pc += 2;
            uint8_t ccc = (opcode >> 3) & 7;
            if (check_condition(*state, ccc)) {
                push_word(state, (state->pc >> 8) & 0xFF, state->pc & 0xFF);
                state->pc = addr;
                cycles = 17;
            } else {
                cycles = 11;
            }
            break;
        }

        case 0xC9: { // RET
            uint8_t high, low;
            pop_word(state, high, low);
            state->pc = ((uint16_t)high << 8) | low;
            cycles = 10;
            break;
        }

        case 0xC0: // RNZ
        case 0xC8: // RZ
        case 0xD0: // RNC
        case 0xD8: // RC
        case 0xE0: // RPO
        case 0xE8: // RPE
        case 0xF0: // RP
        case 0xF8: // RM
        {
            uint8_t ccc = (opcode >> 3) & 7;
            if (check_condition(*state, ccc)) {
                uint8_t high, low;
                pop_word(state, high, low);
                state->pc = ((uint16_t)high << 8) | low;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xC7: // RST 0
        case 0xCF: // RST 1
        case 0xD7: // RST 2
        case 0xDF: // RST 3
        case 0xE7: // RST 4
        case 0xEF: // RST 5
        case 0xF7: // RST 6
        case 0xFF: // RST 7
        {
            uint16_t addr = (opcode & 0x38);
            push_word(state, (state->pc >> 8) & 0xFF, state->pc & 0xFF);
            state->pc = addr;
            cycles = 11;
            break;
        }

        case 0xC1: { // POP B
            uint8_t high, low;
            pop_word(state, high, low);
            state->b = high;
            state->c = low;
            cycles = 10;
            break;
        }

        case 0xD1: { // POP D
            uint8_t high, low;
            pop_word(state, high, low);
            state->d = high;
            state->e = low;
            cycles = 10;
            break;
        }

        case 0xE1: { // POP H
            uint8_t high, low;
            pop_word(state, high, low);
            state->h = high;
            state->l = low;
            cycles = 10;
            break;
        }

        case 0xF1: { // POP PSW
            uint8_t high, low;
            pop_word(state, high, low);
            state->a = high;
            psw_to_flags(*state, low);
            cycles = 10;
            break;
        }

        case 0xC5: // PUSH B
            push_word(state, state->b, state->c);
            cycles = 11;
            break;

        case 0xD5: // PUSH D
            push_word(state, state->d, state->e);
            cycles = 11;
            break;

        case 0xE5: // PUSH H
            push_word(state, state->h, state->l);
            cycles = 11;
            break;

        case 0xF5: // PUSH PSW
            push_word(state, state->a, flags_to_psw(*state));
            cycles = 11;
            break;

        case 0xE3: { // XTHL
            uint8_t h = state->h, l = state->l;
            state->l = state->memory[state->sp];
            state->h = state->memory[state->sp + 1];
            state->memory[state->sp] = l;
            state->memory[state->sp + 1] = h;
            cycles = 18;
            break;
        }

        case 0xEB: { // XCHG
            uint8_t d = state->d, e = state->e;
            state->d = state->h;
            state->e = state->l;
            state->h = d;
            state->l = e;
            cycles = 4;
            break;
        }

        case 0xE9: // PCHL
            state->pc = get_HL(*state);
            cycles = 5;
            break;

        case 0xF9: // SPHL
            state->sp = get_HL(*state);
            cycles = 5;
            break;

        case 0xF3: // DI
            state->int_enable = 0;
            cycles = 4;
            break;

        case 0xFB: // EI
            state->int_enable = 1;
            cycles = 4;
            break;

        case 0xD3: { // OUT d8
            uint8_t port = state->memory[state->pc];
            state->pc++;

            arcade_handle_out(*state, port, state->a);

            cycles = 10;
            break;
        }

        case 0xDB: { // IN d8
            uint8_t port = state->memory[state->pc];
            state->pc++;

            state->a = arcade_handle_in(*state, port);

            cycles = 10;
            break;
        }

        default:
            std::cerr << "Error: Unimplemented opcode 0x" << std::hex << (int)opcode
                      << " at PC = 0x" << (state->pc - 1) << std::dec << std::endl;
            std::exit(1);
    }

    return cycles;
}
