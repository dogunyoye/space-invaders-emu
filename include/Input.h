#pragma once
#include <cstdint>
#include <SDL2/SDL.h>
#include "ArcadeHardware.h"  // port bit masks, set_port_bit
#include "State8080.h"

void handle_key(State8080 &state, SDL_Keycode sym, bool pressed);
void handle_game_button(State8080 &state, int button, bool pressed);
