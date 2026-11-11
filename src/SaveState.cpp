#include "SaveState.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "ArcadeHardware.h"

namespace {

constexpr char kMagic[4] = {'S', 'I', '8', '0'};
constexpr uint8_t kVersion = 1;
// magic + version + 7 regs + sp + pc + flags + int_enable + halted +
// ei_pending + shift_register + shift_offset + out3 + out5
constexpr size_t kHeaderSize = 4 + 1 + 7 + 2 + 2 + 1 + 1 + 1 + 1 + 2 + 1 + 1 + 1;
constexpr size_t kFileSize = kHeaderSize + 65536;

void put16(std::vector<uint8_t> &out, uint16_t v) {
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)(v >> 8));
}

uint16_t get16(const uint8_t *&p) {
    uint16_t v = (uint16_t)(p[0] | (p[1] << 8));
    p += 2;
    return v;
}

}  // namespace

bool save_state(const State8080 &state, const std::string &path) {
    std::vector<uint8_t> out;
    out.reserve(kFileSize);
    out.insert(out.end(), kMagic, kMagic + 4);
    out.push_back(kVersion);
    for (uint8_t r : {state.a, state.b, state.c, state.d, state.e, state.h, state.l})
        out.push_back(r);
    put16(out, state.sp);
    put16(out, state.pc);
    out.push_back((uint8_t)((state.cc.s << 7) | (state.cc.z << 6) | (state.cc.ac << 4) |
                            (state.cc.p << 2) | (state.cc.cy)));
    out.push_back(state.int_enable);
    out.push_back(state.halted);
    out.push_back(state.ei_pending ? 1 : 0);
    put16(out, state.shift_register);
    out.push_back(state.shift_offset);
    out.push_back(arcade_last_out3());
    out.push_back(arcade_last_out5());
    out.insert(out.end(), state.memory.begin(), state.memory.end());

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char *>(out.data()), (std::streamsize)out.size());
    return (bool)f;
}

bool load_state(State8080 &state, const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> in((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (in.size() != kFileSize) return false;
    if (std::memcmp(in.data(), kMagic, 4) != 0 || in[4] != kVersion) return false;

    // Validated: from here on nothing can fail, so it is safe to mutate.
    const uint8_t *p = in.data() + 5;
    state.a = *p++;
    state.b = *p++;
    state.c = *p++;
    state.d = *p++;
    state.e = *p++;
    state.h = *p++;
    state.l = *p++;
    state.sp = get16(p);
    state.pc = get16(p);
    uint8_t flags = *p++;
    state.cc.s = (flags >> 7) & 1;
    state.cc.z = (flags >> 6) & 1;
    state.cc.ac = (flags >> 4) & 1;
    state.cc.p = (flags >> 2) & 1;
    state.cc.cy = flags & 1;
    state.int_enable = *p++;
    state.halted = *p++;
    state.ei_pending = *p++ != 0;
    state.shift_register = get16(p);
    state.shift_offset = (uint8_t)(*p++ & 0x07);
    uint8_t out3 = *p++;
    uint8_t out5 = *p++;
    std::memcpy(state.memory.data(), p, state.memory.size());
    state.emulation_error = false;
    arcade_sound_restore(out3, out5);
    return true;
}
