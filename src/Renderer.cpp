#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include "State8080.h"

// Window dimensions (portrait mode rotated to landscape or rendered natively)
const int SCREEN_WIDTH = 224;
const int SCREEN_HEIGHT = 256;
const int SCALE = 3; // Scale up window size for visibility

void render_vram(SDL_Renderer *renderer, SDL_Texture *texture, const State8080 &state) {
    if (!renderer || !texture) return;
    if (state.memory.size() < 0x4000) return;
    // Pixel buffer for RGBA (224 * 256 * 4 bytes)
    std::vector<uint32_t> pixels(SCREEN_WIDTH * SCREEN_HEIGHT, 0);

    // VRAM starts at 0x2400
    const uint8_t *vram = &state.memory[0x2400];

    for (int i = 0; i < 7168; ++i) {
        uint8_t byte = vram[i];

        // Raw hardware layout is 256x224 (32 bytes per scanline, 224 lines):
        //   byte index i = raw_y * 32 + raw_x / 8, bit = raw_x % 8 (LSB = left).
        // The CRT is mounted rotated 90 deg CCW, so the player sees 224x256:
        //   screen_x = raw_y            (0..223)
        //   screen_y = 255 - raw_x      (0..255, 0 = top)
        // => screen_x = i / 32, screen_y = 255 - ((i % 32) * 8 + bit).
        int screen_x = i / 32;
        int y_base = (i % 32) * 8;

        for (int bit = 0; bit < 8; ++bit) {
            // Check if individual pixel is set (LSB is bottom pixel of chunk)
            bool on = (byte >> bit) & 1;

            int screen_y = 255 - (y_base + bit);

            if (screen_x >= 0 && screen_x < SCREEN_WIDTH && screen_y >= 0 && screen_y < SCREEN_HEIGHT) {
                // Color mapping: Classic arcade white/green lines, or simple white-on-black
                uint32_t color = on ? 0xFFFFFFFF : 0xFF000000; // White on Black
                
                // Optional: Add color overlays if you want authentic arcade colors 
                // (e.g., green filter for the bunker/score area, red for top invasion line)

                pixels[screen_y * SCREEN_WIDTH + screen_x] = color;
            }
        }
    }

    // Update SDL texture with pixel buffer
    SDL_UpdateTexture(texture, NULL, pixels.data(), SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}