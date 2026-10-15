# Space Invaders Emulator (Intel 8080)

A Space Invaders arcade emulator written in C++17 with an SDL2 frontend. Emulates the Intel 8080 CPU at 2 MHz, arcade I/O (shift-register, sound latches, inputs), bitmapped VRAM renderer, and SDL_mixer sound.

## Features

- Full Intel 8080 CPU emulation (`src/Emulator8080.cpp`)
- Space Invaders arcade hardware:
  - IN0/IN1/IN2 inputs, shift-register / shift-offset / shift-data
  - OUT3 / OUT5 sound latches (edge-triggered + UFO loop)
  - RST 1 (mid-screen) / RST 2 (vblank) at 60 FPS
- SDL2 renderer with 90° rotation handling (224×256, 3× scale), classic green/red/white overlay (`--no-color` disables), reused frame buffer, vsync + high-resolution frame pacing
- SDL_mixer sound (accepts MAME-style `0.wav`-`8.wav` or descriptive names)
- 8080 disassembler (`src/Disassembler8080.cpp`, `include/Disassembler8080.h`): `disassemble8080_str()` API plus `--dis <rom>` and `--test --trace` modes
- CP/M diagnostic runner for `CPUTEST.COM`, `TST8080.COM`, `8080PRE.COM`
- Unit tests (`ctest`): `cpu_tests` (flags, DAA/ANA/DCR edge cases, stack wrap), `hw_tests` (shift register, sound latches, ports, disassembler), and `ai_tests` (VRAM vision, chase/dodge/fire, key release)
- Frontend: gamepad input, pause, save/load state (`F5`/`F9` → `save.bin`), ROM size + CRC32 logging, graceful error latch instead of `exit()`

## Requirements

- CMake >= 3.16
- C++17 compiler (GCC / Clang)
- SDL2 >= 2.30
- SDL2_mixer >= 2.8

Ubuntu / WSL:

```bash
sudo apt install cmake libsdl2-dev libsdl2-mixer-dev
```

## Build

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Binary: `build/space_invaders`

## Run

From the project root (so `roms/` and `assets/` resolve):

```bash
./build/space_invaders [--no-color] [--ai-p2]
./build/space_invaders --help
./build/space_invaders --test roms/CPUTEST.COM [--trace]
./build/space_invaders --dis roms/invaders.h 0 64
```

## Controls

| Key | Action |
|-----|--------|
| `C` | Coin |
| `1` | P1 start |
| `2` | P2 start |
| `←` / `→` | P1 left / right |
| `Space` | P1 fire |
| `A` / `D` | P2 left / right |
| `W` | P2 fire |
| `T` | Tilt |
| `P` | Pause / resume |
| `M` | Toggle AI player 2 on/off |
| `F5` / `F9` | Save / load state (`save.bin`) |
| `Esc` | Quit |

AI player 2 (`--ai-p2` or `M`): vision-only bot that drives the P2
left/right/fire inputs from VRAM. It chases the UFO then the lowest
invader row, dodges close enemy bullets, and pulses fire when aligned.
Safe to leave on during P1's turn (the ROM ignores P2 inputs then).
To watch it play: coin (`C`), start P2 (`2`), then enable AI.

Gamepad: DPad = P1 move, A/B/X = P1 fire, Start = P1 start, Back = coin.

DIP defaults: 3 ships, extra ship at 1500, coin info ON in demo.

## Project Layout

```text
CMakeLists.txt
include/
  State8080.h        # CPU + hardware state (array memory, bool flags)
  Emulator8080.h     # emulate8080 / emulate_interrupt (0 = error latch)
  ArcadeHardware.h   # arcade I/O abstraction
  Disassembler8080.h # disassemble8080_str() API
  Renderer.h         # screen constants + render_vram()
  Audio.h          # SDL_mixer frontend
  Input.h            # keyboard + gamepad, port masks
  AIPlayer.h         # AI P2: VRAM vision -> IN2 bits
src/
  main.cpp           # SDL frontend, ROM loading, main loop, --test/--dis modes
  Emulator8080.cpp   # 8080 instruction set (wrapped fetches)
  ArcadeHardware.cpp # ports 0-6, shift register, sound latches
  Renderer.cpp       # VRAM (0x2400) -> SDL texture (overlay colors)
  Disassembler8080.cpp
  Audio.cpp          # sound slots, edge-triggered playback
  Input.cpp          # key/button -> port bits
  AIPlayer.cpp       # AI P2 controller
tests/
  test_cpu.cpp       # cpu_tests: flags, ALU, jumps, stack/fetch wrap
  test_hardware.cpp  # hw_tests: shift reg, sound, ports, disassembler
roms/
  invaders.e/f/g/h   # game ROMs, loaded at 0x0000/0x0800/0x1000/0x1800
  CPUTEST.COM / TST8080.COM / 8080PRE.COM  # CPU diagnostics (loaded at 0x100)
assets/
  0.wav (ufo) / 1.wav (shoot) / 2.wav / 3.wav / 4.wav-7.wav (fleet) / 8.wav ...
  + descriptive aliases: ufo.wav, shoot.wav, explosion.wav, extended.wav
```

## ROM / Asset Notes

The original Space Invaders ROMs remain © Taito / respective rights holders. They are included here for local development / preservation testing only. If you distribute this project, remove `roms/invaders.*` and have users provide their own dumps from original hardware.

Sound samples in `assets/` are third-party / recorded effects. See `LICENSE.md` for code license vs. ROM/asset status.

## Timing Model

- 2 MHz / 60 FPS = 33333 cycles/frame, split in halves around RST 1 / RST 2
- Halted CPU yields its budget until next interrupt (matches hardware); emulation errors stop the frame loop
- Frame pacing: `SDL_RENDERER_PRESENTVSYNC` + `SDL_GetPerformanceCounter` deadline with anti-spiral reset (replaces plain `SDL_Delay`)

## License

Emulator code is MIT — see [LICENSE.md](LICENSE.md).
SDL2 (zlib) and SDL_mixer (zlib) are separate third-party libraries.
