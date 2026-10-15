#pragma once
#include <SDL2/SDL.h>
#include "State8080.h"

inline constexpr int SCREEN_WIDTH = 224;
inline constexpr int SCREEN_HEIGHT = 256;
inline constexpr int SCALE = 3;

// Render VRAM at 0x2400 to the SDL texture (rotated 90 deg CCW).
// Reuses an internal pixel buffer; no allocation per frame.
// When color=true, applies the classic green/red/white overlay zones.
void render_vram(SDL_Renderer *renderer, SDL_Texture *texture, const State8080 &state,
                 bool color = true);
