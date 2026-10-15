#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include "Renderer.h"
#include "State8080.h"

// Classic overlay approximation (screen 224x256, y=0 top):
//   bottom player/bunker band -> green, saucer lane -> red, else white.
static uint32_t overlay_color(int screen_y) {
    if (screen_y >= 192) return 0xFF00FF00;      // green (ABGR)
    if (screen_y >= 64 && screen_y < 96) return 0xFF0000FF;  // red (ABGR)
    return 0xFFFFFFFF;  // white
}

void render_vram(SDL_Renderer *renderer, SDL_Texture *texture, const State8080 &state,
                 bool color) {
    if (!renderer || !texture) return;
    if (state.memory.size() < 0x4000) return;

    // Reused across frames: no per-frame allocation.
    static std::vector<uint32_t> pixels(SCREEN_WIDTH * SCREEN_HEIGHT, 0);

    // VRAM starts at 0x2400
    const uint8_t *vram = state.memory.data() + 0x2400;

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
                uint32_t pixel;
                if (!on) {
                    pixel = 0xFF000000;  // black
                } else if (color) {
                    pixel = overlay_color(screen_y);
                } else {
                    pixel = 0xFFFFFFFF;  // white
                }
                pixels[(size_t)screen_y * SCREEN_WIDTH + screen_x] = pixel;
            }
        }
    }

    // Update SDL texture with pixel buffer
    SDL_UpdateTexture(texture, nullptr, pixels.data(), SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}
