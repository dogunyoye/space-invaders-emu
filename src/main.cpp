#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "ArcadeHardware.h"
#include "Disassembler8080.h"
#include "Emulator8080.h"
#include "State8080.h"

// Forward declaration (implemented in Renderer.cpp)
void render_vram(SDL_Renderer *renderer, SDL_Texture *texture, const State8080 &state);
// Forward declaration (implemented in Emulator8080.cpp)
void emulate_interrupt(State8080 *state, int interrupt_num);
//Forward declaration (implemented in Emulator8080.cpp)
int emulate8080(State8080 *state);

namespace {

const int SCREEN_WIDTH = 224;
const int SCREEN_HEIGHT = 256;
const int SCALE = 3;

// 2 MHz / 60 FPS. Split into two halves around the mid-screen / vblank IRQs.
const int CYCLES_PER_FRAME = 33333;
const int CYCLES_PER_HALF = CYCLES_PER_FRAME / 2;

// IN1 (port 1) bit masks — active high.
const uint8_t IN1_COIN = 0x01;
const uint8_t IN1_P2_START = 0x02;
const uint8_t IN1_P1_START = 0x04;
const uint8_t IN1_P1_SHOOT = 0x10;
const uint8_t IN1_P1_LEFT = 0x20;
const uint8_t IN1_P1_RIGHT = 0x40;

// IN2 (port 2) bit masks — active high (except DIP/coin-info, see below).
const uint8_t IN2_TILT = 0x04;
const uint8_t IN2_P2_SHOOT = 0x10;
const uint8_t IN2_P2_LEFT = 0x20;
const uint8_t IN2_P2_RIGHT = 0x40;
// DIP defaults: 3 ships (bits 0-1 = 00), extra ship at 1500 (bit 3 = 0),
// coin info ON in demo (bit 7 = 0).
const uint8_t IN2_DIP_DEFAULT = 0x00;

// Sound sample slots. Index by MAME-style numbers:
// 0=UFO(loop) 1=shot 2=base hit 3=invader hit 4=extended play
// 5..8=fleet 1..4 (port5 bits 0..3) — plus UFO-hit on port5 bit4.
struct SoundBank {
    std::array<Mix_Chunk *, 10> chunk{};
    int ufo_channel = -1;

    SoundBank() { chunk.fill(nullptr); }
};

SoundBank g_sounds;

std::string exe_dir() {
    char *base = SDL_GetBasePath();
    if (!base) return "";
    std::string d(base);
    SDL_free(base);
    return d;
}

// Try several locations so the game works whether launched from the project
// root, build/, or an installed bin/ dir.
std::string resolve_asset(const std::string &relative) {
    namespace fs = std::filesystem;
    std::vector<std::string> candidates = {
        relative,
        std::string("./") + relative,
        std::string("../") + relative,
    };
    std::string ed = exe_dir();
    if (!ed.empty()) {
        candidates.push_back(ed + relative);
        candidates.push_back(ed + "../" + relative);
        candidates.push_back(ed + "../../" + relative);
    }
    for (const auto &c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return c;
    }
    return relative;  // fall back; caller reports the error
}

bool load_rom(State8080 &state, const std::string &filename, uint16_t offset) {
    std::string path = resolve_asset(filename);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << filename
                  << " (tried " << path << ")" << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (offset + size > static_cast<std::streamsize>(state.memory.size())) {
        std::cerr << "ROM file " << path << " exceeds memory bounds!" << std::endl;
        return false;
    }

    if (file.read(reinterpret_cast<char *>(&state.memory[offset]), size)) {
        std::cout << "Loaded " << path << " at 0x" << std::hex << offset
                  << " (" << std::dec << size << " bytes)" << std::endl;
        return true;
    }
    std::cerr << "Failed to read ROM file: " << path << std::endl;
    return false;
}

void load_sound_slot(int slot, const std::vector<std::string> &names) {
    for (const auto &n : names) {
        std::string path = resolve_asset(n);
        Mix_Chunk *c = Mix_LoadWAV(path.c_str());
        if (c) {
            g_sounds.chunk[slot] = c;
            return;
        }
    }
    // Missing samples are non-fatal: game runs silent for that effect.
    std::cerr << "Warning: sound " << slot << " not found (tried";
    for (const auto &n : names) std::cerr << " " << n;
    std::cerr << "); continuing silent." << std::endl;
}

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

void play_slot(int slot) {
    Mix_Chunk *c = (slot >= 0 && slot < 10) ? g_sounds.chunk[slot] : nullptr;
    if (c) Mix_PlayChannel(-1, c, 0);
}

// Edge-triggered sounds. Port 3 bit 0 (UFO) is level-looped; everything else
// fires on a 0->1 edge.
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

void set_bit(uint8_t &port, uint8_t mask, bool pressed) {
    if (pressed)
        port |= mask;
    else
        port &= (uint8_t)~mask;
}

void handle_key(State8080 &state, SDL_Keycode sym, bool pressed) {
    switch (sym) {
        case SDLK_c:  // coin
            set_bit(state.port1, IN1_COIN, pressed);
            break;
        case SDLK_1:  // P1 start
            set_bit(state.port1, IN1_P1_START, pressed);
            break;
        case SDLK_2:  // P2 start
            set_bit(state.port1, IN1_P2_START, pressed);
            break;
        case SDLK_SPACE:  // P1 shoot
            set_bit(state.port1, IN1_P1_SHOOT, pressed);
            break;
        case SDLK_LEFT:  // P1 left
            set_bit(state.port1, IN1_P1_LEFT, pressed);
            break;
        case SDLK_RIGHT:  // P1 right
            set_bit(state.port1, IN1_P1_RIGHT, pressed);
            break;
        case SDLK_w:  // P2 shoot
            set_bit(state.port2, IN2_P2_SHOOT, pressed);
            break;
        case SDLK_a:  // P2 left
            set_bit(state.port2, IN2_P2_LEFT, pressed);
            break;
        case SDLK_d:  // P2 right
            set_bit(state.port2, IN2_P2_RIGHT, pressed);
            break;
        case SDLK_t:  // tilt
            set_bit(state.port2, IN2_TILT, pressed);
            break;
        default:
            break;
    }
}

// Run all pending cycles up to a budget. A halted CPU just burns cycles
// until the next interrupt (which clears `halted`).
// Stops early on emulation errors (emulate8080 returns <= 0).
void run_cycles(State8080 &state, int budget) {
    int done = 0;
    while (done < budget) {
        if (state.halted || state.emulation_error) break;
        int cycles = emulate8080(&state);
        if (cycles <= 0) break;
        done += cycles;
    }
    if (done >= budget) return;
    // If halted without error, consume the rest as idle time.
    // (No-op: keeps timing model explicit.)
}

int run_diagnostic(const std::string &rom_path, bool trace) {
    State8080 state;

    // CP/M test binaries are loaded at 0x100.
    if (!load_rom(state, rom_path, 0x100)) return 1;

    state.pc = 0x100;
    state.sp = 0xF000;
    // Patch CP/M BDOS entry so CALL 5 returns immediately; we intercept it.
    state.memory[0x0005] = 0xC9;  // RET

    std::cout << "Starting CPU diagnostic: " << rom_path << "\n"
              << "----------------------------------------" << std::endl;

    int total_instructions = 0;
    const int kLimit = 100'000'000;

    while (!state.halted && total_instructions < kLimit) {
        // Intercept CP/M system call (CALL 5).
        if (state.pc == 0x0005) {
            if (state.c == 9) {  // print $-terminated string at DE
                uint16_t addr = (uint16_t)((state.d << 8) | state.e);
                int guard = 0;
                while (state.memory[addr] != '$' && guard++ < 4096) {
                    std::cout << (char)state.memory[addr];
                    addr++;
                }
            } else if (state.c == 2) {  // print char in E
                std::cout << (char)state.e << std::flush;
            }
            uint8_t low = state.memory[state.sp];
            uint8_t high = state.memory[(uint16_t)(state.sp + 1)];
            state.sp = (uint16_t)(state.sp + 2);
            state.pc = (uint16_t)((high << 8) | low);
        }

        if (trace && total_instructions < 200) {
            int len = 1;
            std::cout << disassemble8080_str(state.memory.data(), state.pc, &len) << "\n";
        }
        emulate8080(&state);
        total_instructions++;

        if (state.emulation_error) {
            std::cerr << "\n[Emulation error: opcode 0x" << std::hex
                      << (int)state.error_opcode << " at 0x" << state.error_pc
                      << std::dec << "]" << std::endl;
            break;
        }

        if (state.pc == 0x0000) {
            std::cout << "\n[Test Completed / Program Exited via 0x0000]" << std::endl;
            break;
        }
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Finished after " << total_instructions << " instructions";
    if (total_instructions >= kLimit) std::cout << " (LIMIT REACHED)";
    std::cout << "." << std::endl;
    return 0;
}

int run_disassembly(const std::string &rom_path, uint16_t offset, int count) {
    State8080 state;
    if (!load_rom(state, rom_path, offset)) return 1;
    int pc = offset;
    int end = (count < 0) ? 0x10000 : (offset + count);
    if (end > 0x10000) end = 0x10000;
    // Stop at end of actually-loaded file when count is unlimited: we don't
    // know file size here, so cap by scanning until a run of zeros? Instead
    // just disassemble requested range; caller passes count for raw dumps.
    while (pc < end) {
        int len = 1;
        std::cout << disassemble8080_str(state.memory.data(), pc, &len) << "\n";
        if (len <= 0) len = 1;
        pc += len;
        // For unlimited mode on sparse loads, stop after 0x2000 zero tail
        // is overkill; require explicit count. Break if pc hits 0x10000.
        if (count < 0 && pc >= offset + 0x2000) break;
    }
    return 0;
}

void print_help(const char *prog) {
    std::cout << "Usage:\n"
              << "  " << prog << "                 Run Space Invaders (roms/invaders.e-h)\n"
              << "  " << prog << " --test <rom> [--trace]  Run 8080 CPU diagnostic (CPUTEST.COM, TST8080.COM, 8080PRE.COM)\n"
              << "  " << prog << " --dis <rom> [offset_hex] [count]  Disassemble ROM bytes\n"
              << "  " << prog << " --help          Show this help\n\n"
              << "Keys: C=coin 1=P1 start 2=P2 start  Arrows=P1 move  Space=P1 fire\n"
              << "      W/A/D=P2 fire/left/right  T=tilt  Esc=quit\n";
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "--test") {
            if (argc < 3) {
                std::cerr << "--test requires a ROM path\n";
                return 1;
            }
            bool trace = (argc >= 4 && std::string(argv[3]) == "--trace");
            return run_diagnostic(argv[2], trace);
        }
        if (arg == "--dis") {
            if (argc < 3) {
                std::cerr << "--dis requires a ROM path\n";
                return 1;
            }
            uint16_t offset = 0;
            int count = 64;
            if (argc >= 4) offset = (uint16_t)std::stoul(argv[3], nullptr, 16);
            if (argc >= 5) count = std::stoi(argv[4]);
            return run_disassembly(argv[2], offset, count);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window =
        SDL_CreateWindow("Space Invaders - Intel 8080 Emulator", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE,
                         SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation failed (" << SDL_GetError() << "), trying software."
                  << std::endl;
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                                             SCREEN_HEIGHT);
    if (!texture) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    init_audio();  // non-fatal if samples are missing

    State8080 state;
    state.port1 = 0x00;
    state.port2 = IN2_DIP_DEFAULT;

    if (!load_rom(state, "roms/invaders.h", 0x0000) ||
        !load_rom(state, "roms/invaders.g", 0x0800) ||
        !load_rom(state, "roms/invaders.f", 0x1000) ||
        !load_rom(state, "roms/invaders.e", 0x1800)) {
        std::cerr << "Missing Space Invaders ROMs. Place invaders.e/f/g/h in roms/." << std::endl;
        shutdown_audio();
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool quit = false;
    SDL_Event e;

    const int TARGET_FPS = 60;
    const Uint32 FRAME_TIME_MS = 1000 / TARGET_FPS;

    while (!quit) {
        Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                if (e.key.keysym.sym == SDLK_ESCAPE && e.type == SDL_KEYDOWN) {
                    quit = true;
                    break;
                }
                if (e.key.repeat == 0) handle_key(state, e.key.keysym.sym, e.type == SDL_KEYDOWN);
            }
        }
        if (quit) break;

        // Half-frame of CPU, mid-screen RST 1 (0x08), half-frame, vblank RST 2 (0x10).
        run_cycles(state, CYCLES_PER_HALF);
        emulate_interrupt(&state, 1);
        run_cycles(state, CYCLES_PER_HALF);
        emulate_interrupt(&state, 2);

        poll_arcade_sounds();
        render_vram(renderer, texture, state);

        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_TIME_MS) SDL_Delay(FRAME_TIME_MS - frame_time);
    }

    shutdown_audio();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
