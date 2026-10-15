#pragma once
#include <cstdint>
#include "State8080.h"

// AI player for P2 controls (IN2 bits: left/right/fire).
//
// The original arcade game alternates P1/P2 turns; the ROM only reads the
// P2 inputs when it is player 2's turn, so driving them continuously is
// harmless. Enable with `--ai-p2` or the `M` key at runtime.
//
// Strategy (vision-only, reads VRAM at 0x2400):
//   1. Locate the player cannon in the bottom band (screen coords).
//   2. Pick a target: the UFO saucer lane first, else the lowest invader row.
//   3. Dodge enemy bullets dropping close above the cannon (overrides chase).
//   4. Move toward the target with a deadzone; pulse fire when aligned.
class AIPlayer2 {
   public:
    bool enabled = false;

    void setEnabled(bool e) { enabled = e; }
    void toggle() { enabled = !enabled; }

    // Drive state.port2 P2 bits. No-op when !enabled.
    void update(State8080 &state);

    int lastCannonX() const { return cannon_x_; }
    int lastTargetX() const { return target_x_; }

   private:
    int frame_ = 0;
    int last_known_x_ = 112;  // stable fallback between detections
    int cannon_x_ = -1;
    int target_x_ = -1;
};

// Screen-space helpers (screen_x: 0..223, screen_y: 0..255 with 0 = top).
// Same mapping as Renderer.cpp (rotated 90 deg CCW VRAM layout).
bool ai_pixel(const State8080 &state, int x, int y);
void ai_set_pixel(State8080 &state, int x, int y, bool on);

// Exposed for unit tests.
int ai_find_cannon(const State8080 &state, int fallback_x);
int ai_find_target(const State8080 &state);
int ai_dodge_dir(const State8080 &state, int cannon_x, int target_x);
