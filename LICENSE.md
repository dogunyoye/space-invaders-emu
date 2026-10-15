# MIT License

Copyright (c) 2026 Dimeji

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## Third-party / excluded material

This MIT license applies only to the emulator code in this repository
(`src/`, `include/`, `CMakeLists.txt`, docs).

It does NOT grant rights to:

- `roms/invaders.e`, `roms/invaders.f`, `roms/invaders.g`, `roms/invaders.h` —
  original Space Invaders game code, © Taito / respective rights holders.
  Included for local development / preservation testing only. Redistribute
  only if you hold the rights; otherwise users must supply their own dumps.

- Diagnostic ROMs (`roms/CPUTEST.COM`, `roms/TST8080.COM`, `roms/8080PRE.COM`) —
  retain their original authors' terms.

- Audio in `assets/*.wav` — third-party / recorded effects, not covered by
  this MIT grant unless otherwise marked.

- SDL2 (zlib license) and SDL_mixer (zlib license) — separate libraries
  linked at build time.
