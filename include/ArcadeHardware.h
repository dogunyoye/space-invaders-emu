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
