#include "ArcadeHardware.h"
#include "State8080.h"

// Latched sound-port state. The 8080 OUTs just latch a byte; the analog
// circuitry (or our SDL frontend) interprets 0->1 edges as triggers.
static uint8_t s_out3 = 0;
static uint8_t s_out3_prev = 0;
static uint8_t s_out5 = 0;
static uint8_t s_out5_prev = 0;

void arcade_sound_reset() {
    s_out3 = s_out3_prev = 0;
    s_out5 = s_out5_prev = 0;
}

uint8_t arcade_last_out3() { return s_out3; }
uint8_t arcade_last_out3_prev() { return s_out3_prev; }
uint8_t arcade_last_out5() { return s_out5; }
uint8_t arcade_last_out5_prev() { return s_out5_prev; }
void arcade_ack_sound() {
    s_out3_prev = s_out3;
    s_out5_prev = s_out5;
}

uint8_t arcade_handle_in(const State8080 &state, uint8_t port) {
    switch (port) {
        case 0:
            // Mapped in hardware but never used by the ROM.
            // Bits 1-3 are hardwired high (computerarcheology.com).
            return 0x0E;
        case 1:
            // Bit 3 is hardwired high, bit 7 unconnected (reads 0).
            // state.port1 carries coin/start/P1 buttons.
            return (uint8_t)((state.port1 | 0x08) & 0x7F);
        case 2:
            // state.port2 carries DIP + tilt + P2 buttons.
            return state.port2;
        case 3: {
            // Shift-register result:
            //   v = shift_register >> (8 - shift_offset)
            uint16_t v = (uint16_t)(state.shift_register >> (8 - state.shift_offset));
            return (uint8_t)(v & 0xFF);
        }
        default:
            return 0;
    }
}

void arcade_handle_out(State8080 &state, uint8_t port, uint8_t value) {
    switch (port) {
        case 2:
            // Only bits 0-2 are the shift amount.
            state.shift_offset = (uint8_t)(value & 0x07);
            break;
        case 3:
            s_out3_prev = s_out3;
            s_out3 = value;
            break;
        case 4:
            // 16-bit queue: old MSB -> LSB, new byte -> MSB.
            // $0000, write $AA -> $AA00, write $FF -> $FFAA.
            state.shift_register =
                (uint16_t)((state.shift_register >> 8) | ((uint16_t)value << 8));
            break;
        case 5:
            s_out5_prev = s_out5;
            s_out5 = value;
            break;
        case 6:
            // Watchdog reset — nothing to emulate.
            break;
        default:
            break;
    }
}
