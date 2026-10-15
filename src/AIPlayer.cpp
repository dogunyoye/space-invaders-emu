#include "AIPlayer.h"

#include <algorithm>
#include <cstdlib>

#include "Input.h"
#include "Renderer.h"  // SCREEN_WIDTH / SCREEN_HEIGHT

namespace {

// --- Tunables (screen coords, y=0 top) ---
constexpr int kPlayerTop = 218;
constexpr int kPlayerBottom = 248;
constexpr int kPlayerMinHeight = 4;  // filters the 1px ground line
constexpr int kCannonMinWidth = 8;

constexpr int kInvaderTop = 24;
constexpr int kInvaderBottom = 180;
constexpr int kSaucerTop = 56;
constexpr int kSaucerBottom = 96;

constexpr int kDodgeTop = 140;
constexpr int kDodgeBottom = 205;
constexpr int kDodgeRadius = 12;
constexpr int kDodgeReactY = 160;  // ignore far bullets above this line

constexpr int kDeadzone = 3;
constexpr int kFireWindow = 4;

}  // namespace

bool ai_pixel(const State8080 &state, int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return false;
    if (state.memory.size() < 0x4000) return false;
    const int raw = 255 - y;  // 0..255
    const int off = raw / 8;
    const int bit = raw % 8;
    const int i = x * 32 + off;
    // VRAM is 7168 bytes at 0x2400.
    if (i < 0 || i >= 7168) return false;
    return (state.memory[(size_t)0x2400 + i] >> bit) & 1;
}

void ai_set_pixel(State8080 &state, int x, int y, bool on) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    const int raw = 255 - y;
    const int off = raw / 8;
    const int bit = raw % 8;
    const int i = x * 32 + off;
    if (i < 0 || i >= 7168) return;
    uint8_t &byte = state.memory[(size_t)0x2400 + i];
    if (on)
        byte |= (uint8_t)(1u << bit);
    else
        byte &= (uint8_t)~(1u << bit);
}

int ai_find_cannon(const State8080 &state, int fallback_x) {
    // Column occupancy in the player band.
    int best_x = -1;
    int best_score = 0;  // widest run
    int best_dist = 1 << 30;

    int run_start = -1;
    auto flush_run = [&](int run_end) {
        if (run_start < 0) return;
        const int width = run_end - run_start;
        if (width < kCannonMinWidth) {
            run_start = -1;
            return;
        }
        const int center = (run_start + run_end - 1) / 2;
        // Prefer the widest run; break ties by closeness to fallback.
        const int dist = std::abs(center - fallback_x);
        if (width > best_score || (width == best_score && dist < best_dist)) {
            best_score = width;
            best_dist = dist;
            best_x = center;
        }
        run_start = -1;
    };

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        int count = 0;
        for (int y = kPlayerTop; y <= kPlayerBottom; ++y) {
            if (ai_pixel(state, x, y)) ++count;
        }
        if (count >= kPlayerMinHeight) {
            if (run_start < 0) run_start = x;
        } else {
            flush_run(x);
        }
    }
    flush_run(SCREEN_WIDTH);
    return best_x;
}

int ai_find_target(const State8080 &state) {
    // 1. UFO saucer lane: single wide blob. Prefer it (bonus points).
    {
        int run_start = -1;
        int best_x = -1;
        int best_w = 0;
        auto flush = [&](int end) {
            if (run_start < 0) return;
            const int w = end - run_start;
            if (w >= 8 && w <= 24 && w > best_w) {
                best_w = w;
                best_x = (run_start + end - 1) / 2;
            }
            run_start = -1;
        };
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            int count = 0;
            for (int y = kSaucerTop; y <= kSaucerBottom; ++y) {
                if (ai_pixel(state, x, y)) ++count;
            }
            if (count >= 3) {
                if (run_start < 0) run_start = x;
            } else {
                flush(x);
            }
        }
        flush(SCREEN_WIDTH);
        if (best_x >= 0) return best_x;
    }

    // 2. Lowest invader row: find the lowest occupied row, average its Xs.
    int y_low = -1;
    for (int y = kInvaderBottom; y >= kInvaderTop; --y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            if (ai_pixel(state, x, y)) {
                y_low = y;
                break;
            }
        }
        if (y_low >= 0) break;
    }
    if (y_low < 0) return -1;

    const int y0 = std::max(kInvaderTop, y_low - 10);
    long sum = 0;
    int n = 0;
    for (int y = y0; y <= y_low; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            if (ai_pixel(state, x, y)) {
                sum += x;
                ++n;
            }
        }
    }
    if (n == 0) return -1;
    return (int)(sum / n);
}

int ai_dodge_dir(const State8080 &state, int cannon_x, int target_x) {
    if (cannon_x < 0) return 0;
    // Column occupancy in the dodge window. Enemy shots are narrow vertical
    // streaks (<=3px wide); the fleet/bunkers are wide blobs, so runs wider
    // than that are ignored rather than treated as bullets.
    constexpr int kWidth = 2 * kDodgeRadius + 1;
    int col[kWidth] = {};
    int col_top[kWidth];
    for (int i = 0; i < kWidth; ++i) col_top[i] = -1;
    for (int i = 0; i < kWidth; ++i) {
        const int x = cannon_x - kDodgeRadius + i;
        int count = 0;
        for (int y = kDodgeTop; y <= kDodgeBottom; ++y) {
            if (ai_pixel(state, x, y)) {
                ++count;
                col_top[i] = y;  // last (lowest) occupied row in this column
            }
        }
        col[i] = count;
    }
    int left = 0, right = 0, center = 0;
    int bullet_closest = -1;
    int i = 0;
    while (i < kWidth) {
        if (col[i] < 4) {
            ++i;
            continue;
        }
        int j = i;
        while (j < kWidth && col[j] >= 4) ++j;
        const int run_w = j - i;  // consecutive bullet-like columns
        if (run_w <= 3) {
            const int run_center = (i + j - 1) / 2 - kDodgeRadius;  // dx
            for (int k = i; k < j; ++k) bullet_closest = std::max(bullet_closest, col_top[k]);
            if (run_center <= -2)
                ++left;
            else if (run_center >= 2)
                ++right;
            else
                ++center;
        }
        i = j;
    }
    if (bullet_closest < kDodgeReactY) return 0;  // too far to matter
    if (left == 0 && right == 0 && center == 0) return 0;
    if (center > 0) {
        // Dead-center threat: sidestep toward the target if possible,
        // else away from the denser side.
        if (target_x > cannon_x) return +1;
        if (target_x < cannon_x) return -1;
        return (left >= right) ? +1 : -1;
    }
    if (left > 0 && right == 0) return +1;  // threat left -> go right
    if (right > 0 && left == 0) return -1;  // threat right -> go left
    // Threats both sides: move toward the sparser side.
    return (left >= right) ? +1 : -1;
}

void AIPlayer2::update(State8080 &state) {
    if (!enabled) return;
    ++frame_;

    const int cannon = ai_find_cannon(state, last_known_x_);
    if (cannon >= 0) {
        last_known_x_ = cannon;
        cannon_x_ = cannon;
    } else {
        cannon_x_ = -1;  // e.g. attract mode / player exploding: release keys
        set_port_bit(state.port2, IN2_P2_LEFT, false);
        set_port_bit(state.port2, IN2_P2_RIGHT, false);
        set_port_bit(state.port2, IN2_P2_SHOOT, false);
        return;
    }

    const int target = ai_find_target(state);
    target_x_ = target;

    int dir = 0;
    const int dodge = ai_dodge_dir(state, cannon, target);
    if (dodge != 0) {
        dir = dodge;
    } else if (target >= 0) {
        const int dx = target - cannon;
        if (dx > kDeadzone)
            dir = +1;
        else if (dx < -kDeadzone)
            dir = -1;
    }

    // Don't drive into the walls.
    if ((cannon <= 2 && dir < 0) || (cannon >= SCREEN_WIDTH - 3 && dir > 0)) dir = 0;

    set_port_bit(state.port2, IN2_P2_LEFT, dir < 0);
    set_port_bit(state.port2, IN2_P2_RIGHT, dir > 0);

    // Pulse fire when aligned so the ROM sees fresh edges (it allows a
    // single player shot in flight, so holding would also work, but pulsing
    // re-triggers reliably after each shot resolves).
    bool want_fire = false;
    if (target >= 0 && std::abs(target - cannon) <= kFireWindow) {
        want_fire = (frame_ % 12) < 6;
    }
    set_port_bit(state.port2, IN2_P2_SHOOT, want_fire);
}
