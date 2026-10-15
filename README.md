# Space Invaders Emulator (Intel 8080)

A Space Invaders arcade emulator written in C++17 with an SDL2 frontend. Emulates the Intel 8080 CPU at 2 MHz, arcade I/O (shift-register, sound latches, inputs), bitmapped VRAM renderer, and SDL_mixer sound.

## Features

- Full Intel 8080 CPU emulation (`src/Emulator8080.cpp`)
- Space Invaders arcade hardware:
  - IN0/IN1/IN2 inputs, shift-register / shift-offset / shift-data
  - OUT3 / OUT5 sound latches (edge-triggered + UFO loop)
  - RST 1 (mid-screen) / RST 2 (vblank) at 60 FPS
- SDL2 renderer with 90° rotation handling (224×256, 3× scale)
- SDL_mixer sound (accepts MAME-style `0.wav`-`8.wav` or descriptive names)
- 8080 disassembler (`src/Disassembler8080.cpp`)
- CP/M diagnostic runner for `CPUTEST.COM`, `TST8080.COM`, `8080PRE.COM`

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
```

Binary: `build/space_invaders`

## Run

From the project root (so `roms/` and `assets/` resolve):

```bash
./build/space_invaders
./build/space_invaders --help
./build/space_invaders --test roms/CPUTEST.COM
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
| `Esc` | Quit |

DIP defaults: 3 ships, extra ship at 1500, coin info ON in demo.

## Project Layout

```text
CMakeLists.txt
include/
  State8080.h        # CPU + hardware state
  Emulator8080.h     # emulate8080 / emulate_interrupt
  ArcadeHardware.h   # arcade I/O abstraction
src/
  main.cpp           # SDL frontend, ROM/sound loading, main loop, --test mode
  Emulator8080.cpp   # 8080 instruction set
  ArcadeHardware.cpp # ports 0-6, shift register, sound latches
  Renderer.cpp       # VRAM (0x2400) -> SDL texture
  Disassembler8080.cpp
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
- Halted CPU burns cycles until next interrupt (matches hardware)

## License

Emulator code is MIT — see [LICENSE.md](LICENSE.md).
SDL2 (zlib) and SDL_mixer (zlib) are separate third-party libraries.
