#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "ArcadeHardware.h"
#include "AIPlayer.h"
#include "Audio.h"
#include "Disassembler8080.h"
#include "Emulator8080.h"
#include "Input.h"
#include "Renderer.h"
#include "State8080.h"

namespace {

// 2 MHz / 60 FPS. Split into two halves around the mid-screen / vblank IRQs.
const int CYCLES_PER_FRAME = 33333;
const int CYCLES_PER_HALF = CYCLES_PER_FRAME / 2;

uint32_t crc32_bytes(const uint8_t *data, size_t len);

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
    if (size != 2048 && offset < 0x2000) {
        std::cerr << "Warning: " << path << " size " << size
                  << " != 2048 (expected invaders chunk)" << std::endl;
    }

    if (file.read(reinterpret_cast<char *>(&state.memory[offset]), size)) {
        uint32_t crc = crc32_bytes(&state.memory[offset], (size_t)size);
        std::cout << "Loaded " << path << " at 0x" << std::hex << offset
                  << " (" << std::dec << size << " bytes, crc32=0x" << std::hex << crc << std::dec << ")"
                  << std::endl;
        return true;
    }
    std::cerr << "Failed to read ROM file: " << path << std::endl;
    return false;
}

// CRC32 (IEEE) for ROM validation logging.
uint32_t crc32_bytes(const uint8_t *data, size_t len) {    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return ~crc;
}

bool save_state(const State8080 &state, const std::string &path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char *>(&state.a), 1);
    f.write(reinterpret_cast<const char *>(&state.b), 1);
    f.write(reinterpret_cast<const char *>(&state.c), 1);
    f.write(reinterpret_cast<const char *>(&state.d), 1);
    f.write(reinterpret_cast<const char *>(&state.e), 1);
    f.write(reinterpret_cast<const char *>(&state.h), 1);
    f.write(reinterpret_cast<const char *>(&state.l), 1);
    f.write(reinterpret_cast<const char *>(&state.sp), 2);
    f.write(reinterpret_cast<const char *>(&state.pc), 2);
    uint8_t flags = (uint8_t)((state.cc.s << 7) | (state.cc.z << 6) | (state.cc.ac << 4) |
                             (state.cc.p << 2) | (state.cc.cy));
    f.write(reinterpret_cast<const char *>(&flags), 1);
    f.write(reinterpret_cast<const char *>(&state.int_enable), 1);
    f.write(reinterpret_cast<const char *>(&state.port1), 1);
    f.write(reinterpret_cast<const char *>(&state.port2), 1);
    f.write(reinterpret_cast<const char *>(&state.shift_register), 2);
    f.write(reinterpret_cast<const char *>(&state.shift_offset), 1);
    f.write(reinterpret_cast<const char *>(state.memory.data()), state.memory.size());
    return (bool)f;
}

bool load_state(State8080 &state, const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    uint8_t flags = 0;
    f.read(reinterpret_cast<char *>(&state.a), 1);
    f.read(reinterpret_cast<char *>(&state.b), 1);
    f.read(reinterpret_cast<char *>(&state.c), 1);
    f.read(reinterpret_cast<char *>(&state.d), 1);
    f.read(reinterpret_cast<char *>(&state.e), 1);
    f.read(reinterpret_cast<char *>(&state.h), 1);
    f.read(reinterpret_cast<char *>(&state.l), 1);
    f.read(reinterpret_cast<char *>(&state.sp), 2);
    f.read(reinterpret_cast<char *>(&state.pc), 2);
    f.read(reinterpret_cast<char *>(&flags), 1);
    f.read(reinterpret_cast<char *>(&state.int_enable), 1);
    f.read(reinterpret_cast<char *>(&state.port1), 1);
    f.read(reinterpret_cast<char *>(&state.port2), 1);
    f.read(reinterpret_cast<char *>(&state.shift_register), 2);
    f.read(reinterpret_cast<char *>(&state.shift_offset), 1);
    f.read(reinterpret_cast<char *>(state.memory.data()), state.memory.size());
    state.cc.s = (flags >> 7) & 1;
    state.cc.z = (flags >> 6) & 1;
    state.cc.ac = (flags >> 4) & 1;
    state.cc.p = (flags >> 2) & 1;
    state.cc.cy = flags & 1;
    state.halted = 0;
    state.emulation_error = false;
    return (bool)f;
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
              << "  " << prog << " [--no-color] [--ai-p2]  Disable overlay colors / enable AI P2\n"
              << "  " << prog << " --test <rom> [--trace]  Run 8080 CPU diagnostic (CPUTEST.COM, TST8080.COM, 8080PRE.COM)\n"
              << "  " << prog << " --dis <rom> [offset_hex] [count]  Disassemble ROM bytes\n"
              << "  " << prog << " --help          Show this help\n\n"
              << "Keys: C=coin 1=P1 start 2=P2 start  Arrows=P1 move  Space=P1 fire\n"
              << "      W/A/D=P2 fire/left/right  T=tilt  P=pause  F5=save F9=load  M=AI P2 on/off  Esc=quit\n"
              << "Gamepad: DPad=move A/B/X=fire Start=start Back=coin\n";
}

}  // namespace

int main(int argc, char *argv[]) {
    bool use_color = true;
    bool ai_p2 = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-color") use_color = false;
        if (std::string(argv[i]) == "--ai-p2") ai_p2 = true;
    }
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
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

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    SDL_GameController *pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) {
                std::cout << "Gamepad: " << SDL_GameControllerName(pad) << std::endl;
                break;
            }
        }
    }

    bool quit = false;
    bool paused = false;
    const std::string save_path = "save.bin";
    SDL_Event e;

    AIPlayer2 ai_p2_player;
    ai_p2_player.setEnabled(ai_p2);
    if (ai_p2_player.enabled) std::cout << "[AI P2 enabled] (M to toggle)" << std::endl;

    const double TARGET_FPS = 60.0;
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const Uint64 frame_ticks = (Uint64)(freq / TARGET_FPS);
    Uint64 next_frame = SDL_GetPerformanceCounter();

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                    break;
                }
                if (e.key.keysym.sym == SDLK_p) {
                    paused = !paused;
                    std::cout << (paused ? "[paused]" : "[resumed]") << std::endl;
                }
                if (e.key.keysym.sym == SDLK_F5) {
                    std::cout << (save_state(state, save_path) ? "Saved " : "Save failed ") << save_path
                              << std::endl;
                }
                if (e.key.keysym.sym == SDLK_F9) {
                    std::cout << (load_state(state, save_path) ? "Loaded " : "Load failed ")
                              << save_path << std::endl;
                }
                if (e.key.keysym.sym == SDLK_m) {
                    ai_p2_player.toggle();
                    std::cout << (ai_p2_player.enabled ? "[AI P2 enabled]" : "[AI P2 disabled]")
                              << std::endl;
                    if (!ai_p2_player.enabled) {
                        // Release AI-driven P2 bits so the cannon doesn't stick.
                        set_port_bit(state.port2, IN2_P2_LEFT, false);
                        set_port_bit(state.port2, IN2_P2_RIGHT, false);
                        set_port_bit(state.port2, IN2_P2_SHOOT, false);
                    }
                } else {
                    handle_key(state, e.key.keysym.sym, true);
                }
            } else if (e.type == SDL_KEYUP) {
                handle_key(state, e.key.keysym.sym, false);
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
                handle_game_button(state, e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN);
            }
        }
        if (quit) break;

        if (!paused) {
            // AI P2 drives IN2 left/right/fire before the CPU reads them.
            if (ai_p2_player.enabled) ai_p2_player.update(state);
            // Half-frame of CPU, mid-screen RST 1 (0x08), half-frame, vblank RST 2 (0x10).
            run_cycles(state, CYCLES_PER_HALF);
            emulate_interrupt(&state, 1);
            run_cycles(state, CYCLES_PER_HALF);
            emulate_interrupt(&state, 2);
            poll_arcade_sounds();
        }
        render_vram(renderer, texture, state, use_color);

        next_frame += frame_ticks;
        Uint64 now = SDL_GetPerformanceCounter();
        if (next_frame > now) {
            Uint32 wait_ms = (Uint32)((next_frame - now) * 1000 / freq);
            if (wait_ms > 0) SDL_Delay(wait_ms);
        } else {
            // Running behind (vsync off / slow frame): reset deadline to avoid spiral.
            next_frame = now;
        }
        if (state.emulation_error) {
            std::cerr << "Emulation error: opcode 0x" << std::hex
                      << (int)state.error_opcode << " at 0x" << state.error_pc
                      << std::dec << " — stopping." << std::endl;
            break;
        }
    }

    if (pad) SDL_GameControllerClose(pad);
    shutdown_audio();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
