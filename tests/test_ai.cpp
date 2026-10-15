#include <cstdio>
#include "AIPlayer.h"
#include "Input.h"
#include "State8080.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void draw_block(State8080 &s, int x0, int x1, int y0, int y1) {
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y) ai_set_pixel(s, x, y, true);
}

static void test_pixel_roundtrip() {
    State8080 s;
    ai_set_pixel(s, 10, 20, true);
    CHECK(ai_pixel(s, 10, 20));
    CHECK(!ai_pixel(s, 11, 20));
    ai_set_pixel(s, 10, 20, false);
    CHECK(!ai_pixel(s, 10, 20));
    CHECK(!ai_pixel(s, -1, 0));
    CHECK(!ai_pixel(s, 224, 0));
}

static void test_cannon_detect() {
    State8080 s;
    draw_block(s, 44, 56, 225, 232);  // 13-wide cannon
    CHECK(ai_find_cannon(s, 112) == 50);
    // Ground line alone (1px tall, full width) must not count as a cannon.
    State8080 g;
    draw_block(g, 0, 223, 250, 250);
    CHECK(ai_find_cannon(g, 112) < 0);
}

static void test_chase_right() {
    State8080 s;
    draw_block(s, 44, 56, 225, 232);    // cannon at x=50
    draw_block(s, 96, 104, 170, 176);  // invader row low, centered x=100
    AIPlayer2 ai;
    ai.setEnabled(true);
    ai.update(s);
    CHECK(s.port2 & IN2_P2_RIGHT);
    CHECK(!(s.port2 & IN2_P2_LEFT));
}

static void test_chase_left() {
    State8080 s;
    draw_block(s, 144, 156, 225, 232);  // cannon at x=150
    draw_block(s, 96, 104, 170, 176);   // invader at x=100
    AIPlayer2 ai;
    ai.setEnabled(true);
    ai.update(s);
    CHECK(s.port2 & IN2_P2_LEFT);
    CHECK(!(s.port2 & IN2_P2_RIGHT));
}

static void test_fire_when_aligned() {
    State8080 s;
    draw_block(s, 94, 106, 225, 232);   // cannon at x=100
    draw_block(s, 96, 104, 170, 176);   // invader at x=100
    AIPlayer2 ai;
    ai.setEnabled(true);
    bool fired = false;
    for (int i = 0; i < 12; ++i) {
        ai.update(s);
        if (s.port2 & IN2_P2_SHOOT) fired = true;
    }
    CHECK(fired);
    // No stray movement when aligned.
    CHECK(!(s.port2 & IN2_P2_LEFT));
    CHECK(!(s.port2 & IN2_P2_RIGHT));
}

static void test_dodge() {
    State8080 s;
    draw_block(s, 94, 106, 225, 232);  // cannon at x=100
    draw_block(s, 96, 104, 120, 126);  // invader far above (target aligned)
    draw_block(s, 88, 90, 180, 190);   // enemy bullet left of cannon, close
    int dir = ai_dodge_dir(s, 100, 100);
    CHECK(dir == 1);  // threat on left -> move right
    AIPlayer2 ai;
    ai.setEnabled(true);
    ai.update(s);
    CHECK(s.port2 & IN2_P2_RIGHT);
}

static void test_no_cannon_releases_keys() {
    State8080 s;  // empty VRAM: attract mode / explosion
    s.port2 = (uint8_t)(IN2_P2_LEFT | IN2_P2_SHOOT);
    AIPlayer2 ai;
    ai.setEnabled(true);
    ai.update(s);
    CHECK(!(s.port2 & IN2_P2_LEFT));
    CHECK(!(s.port2 & IN2_P2_RIGHT));
    CHECK(!(s.port2 & IN2_P2_SHOOT));
}

static void test_disabled_noop() {
    State8080 s;
    draw_block(s, 44, 56, 225, 232);
    draw_block(s, 96, 104, 170, 176);
    AIPlayer2 ai;  // disabled by default
    ai.update(s);
    CHECK(s.port2 == 0);
}

int main() {
    test_pixel_roundtrip();
    test_cannon_detect();
    test_chase_right();
    test_chase_left();
    test_fire_when_aligned();
    test_dodge();
    test_no_cannon_releases_keys();
    test_disabled_noop();
    std::printf("ai: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
