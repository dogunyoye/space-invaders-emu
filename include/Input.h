#pragma once
#include <cstdint>
#include <SDL2/SDL.h>
#include "State8080.h"

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

void set_port_bit(uint8_t &port, uint8_t mask, bool pressed);
void handle_key(State8080 &state, SDL_Keycode sym, bool pressed);
void handle_game_button(State8080 &state, int button, bool pressed);
