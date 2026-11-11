#pragma once
#include <cstdint>
#include "State8080.h"

// Space Invaders arcade I/O abstraction.
//
// Ports (8080 I/O space):
//   Read  0 -> IN0 (unused by ROM, bits 1-3 hardwired high)
//   Read  1 -> IN1 (P1 controls + coin/start)
//   Read  2 -> IN2 (DIP switches + P2 controls + tilt)
//   Read  3 -> shift-register result
//   Write 2 -> shift amount (bits 0-2)
//   Write 3 -> sound bank 1 (discrete analog triggers)
//   Write 4 -> shift data
//   Write 5 -> sound bank 2
//   Write 6 -> watchdog (ignored by emulator)

// IN1 (port 1) bit masks — active high.
inline constexpr uint8_t IN1_COIN = 0x01;
inline constexpr uint8_t IN1_P2_START = 0x02;
inline constexpr uint8_t IN1_P1_START = 0x04;
inline constexpr uint8_t IN1_P1_SHOOT = 0x10;
inline constexpr uint8_t IN1_P1_LEFT = 0x20;
inline constexpr uint8_t IN1_P1_RIGHT = 0x40;

// IN2 (port 2) bit masks — active high (except DIP/coin-info, see below).
inline constexpr uint8_t IN2_TILT = 0x04;
inline constexpr uint8_t IN2_P2_SHOOT = 0x10;
inline constexpr uint8_t IN2_P2_LEFT = 0x20;
inline constexpr uint8_t IN2_P2_RIGHT = 0x40;
// DIP defaults: 3 ships (bits 0-1 = 00), extra ship at 1500 (bit 3 = 0),
// coin info ON in demo (bit 7 = 0).
inline constexpr uint8_t IN2_DIP_DEFAULT = 0x00;

inline void set_port_bit(uint8_t &port, uint8_t mask, bool pressed) {
    if (pressed)
        port |= mask;
    else
        port &= (uint8_t)~mask;
}

// Memory map: ROM 0x0000-0x1FFF (write-protected), RAM 0x2000-0x3FFF,
// mirrored above 0x4000 (only 14 address lines are decoded).
void arcade_init_memory_map(State8080 &state);

uint8_t arcade_handle_in(const State8080 &state, uint8_t port);
void arcade_handle_out(State8080 &state, uint8_t port, uint8_t value);

// Sound latch access for the frontend (SDL_mixer).
// OUT 3 / OUT 5 are level-latched; the frontend should play a sample on a
// 0->1 edge (except bit 0 of port 3 = looping UFO sound).
uint8_t arcade_last_out3();
uint8_t arcade_last_out3_prev();
uint8_t arcade_last_out5();
uint8_t arcade_last_out5_prev();
void arcade_ack_sound();  // mark current sound state as consumed
void arcade_sound_reset();
// Restore latched levels (save states). Leaves *_prev alone so the next
// poll starts/stops sounds relative to what is currently playing.
void arcade_sound_restore(uint8_t out3, uint8_t out5);
