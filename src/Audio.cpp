#include "Audio.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "ArcadeHardware.h"

namespace {

// Sound sample slots. Index by MAME-style numbers:
// 0=UFO(loop) 1=shot 2=base hit 3=invader hit 4=extended play
// 5..8=fleet 1..4 (port5 bits 0..3) — plus UFO-hit on port5 bit4.
struct SoundBank {
    std::array<Mix_Chunk *, 10> chunk{};
    int ufo_channel = -1;

    SoundBank() { chunk.fill(nullptr); }
};

SoundBank g_sounds;

std::string audio_exe_dir() {
    char *base = SDL_GetBasePath();
    if (!base) return "";
    std::string d(base);
    SDL_free(base);
    return d;
}

std::string audio_resolve(const std::string &relative) {
    namespace fs = std::filesystem;
    std::vector<std::string> candidates = {
        relative,
        std::string("./") + relative,
        std::string("../") + relative,
    };
    std::string ed = audio_exe_dir();
    if (!ed.empty()) {
        candidates.push_back(ed + relative);
        candidates.push_back(ed + "../" + relative);
        candidates.push_back(ed + "../../" + relative);
    }
    for (const auto &c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return c;
    }
    return relative;
}

void load_sound_slot(int slot, const std::vector<std::string> &names) {
    for (const auto &n : names) {
        std::string path = audio_resolve(n);
        Mix_Chunk *c = Mix_LoadWAV(path.c_str());
        if (c) {
            g_sounds.chunk[(size_t)slot] = c;
            return;
        }
    }
    std::cerr << "Warning: sound " << slot << " not found (tried";
    for (const auto &n : names) std::cerr << " " << n;
    std::cerr << "); continuing silent." << std::endl;
}

void play_slot(int slot) {
    Mix_Chunk *c = (slot >= 0 && slot < 10) ? g_sounds.chunk[(size_t)slot] : nullptr;
    if (c) Mix_PlayChannel(-1, c, 0);
}

}  // namespace

bool init_audio() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "Mix_OpenAudio failed: " << Mix_GetError()
                  << " (continuing without audio)" << std::endl;
        return false;
    }
    Mix_AllocateChannels(16);

    // Accept both MAME-style numeric names and descriptive names.
    load_sound_slot(0, {"assets/0.wav", "assets/ufo.wav"});
    load_sound_slot(1, {"assets/1.wav", "assets/shoot.wav"});
    load_sound_slot(2, {"assets/2.wav", "assets/explosion.wav", "assets/basehit.wav"});
    load_sound_slot(3, {"assets/3.wav", "assets/invhit.wav"});
    load_sound_slot(4, {"assets/extended.wav", "assets/4_extend.wav"});
    load_sound_slot(5, {"assets/4.wav", "assets/walk1.wav"});
    load_sound_slot(6, {"assets/5.wav", "assets/walk2.wav"});
    load_sound_slot(7, {"assets/6.wav", "assets/walk3.wav"});
    load_sound_slot(8, {"assets/7.wav", "assets/walk4.wav"});
    load_sound_slot(9, {"assets/8.wav", "assets/ufohit.wav"});
    arcade_sound_reset();
    return true;
}

void poll_arcade_sounds() {
    uint8_t o3 = arcade_last_out3();
    uint8_t p3 = arcade_last_out3_prev();
    uint8_t o5 = arcade_last_out5();
    uint8_t p5 = arcade_last_out5_prev();
    uint8_t edge3 = (uint8_t)((o3 ^ p3) & o3);
    uint8_t edge5 = (uint8_t)((o5 ^ p5) & o5);

    // UFO looping sound.
    bool ufo_on = (o3 & 0x01) != 0;
    bool ufo_was = (p3 & 0x01) != 0;
    if (ufo_on && !ufo_was) {
        if (g_sounds.chunk[0])
            g_sounds.ufo_channel = Mix_PlayChannel(-1, g_sounds.chunk[0], -1);
    } else if (!ufo_on && ufo_was) {
        if (g_sounds.ufo_channel >= 0) {
            Mix_HaltChannel(g_sounds.ufo_channel);
            g_sounds.ufo_channel = -1;
        }
    }

    if (edge3 & 0x02) play_slot(1);  // shot
    if (edge3 & 0x04) play_slot(2);  // base hit
    if (edge3 & 0x08) play_slot(3);  // invader hit
    if (edge3 & 0x10) play_slot(4);  // extended play
    if (edge5 & 0x01) play_slot(5);  // fleet 1
    if (edge5 & 0x02) play_slot(6);  // fleet 2
    if (edge5 & 0x04) play_slot(7);  // fleet 3
    if (edge5 & 0x08) play_slot(8);  // fleet 4
    if (edge5 & 0x10) play_slot(9);  // UFO hit

    arcade_ack_sound();
}

void shutdown_audio() {
    if (g_sounds.ufo_channel >= 0) Mix_HaltChannel(g_sounds.ufo_channel);
    for (auto *c : g_sounds.chunk) {
        if (c) Mix_FreeChunk(c);
    }
    g_sounds.chunk.fill(nullptr);
    Mix_CloseAudio();
}
