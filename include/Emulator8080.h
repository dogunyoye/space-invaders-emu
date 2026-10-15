#pragma once
#include "State8080.h"

// Execute a single 8080 instruction at state->pc.
// Returns the number of cycles consumed.
int emulate8080(State8080 *state);

// Generate an 8080 RST interrupt (Space Invaders uses RST 1 and RST 2).
// No-op when interrupts are disabled (int_enable == 0).
// Clears the HLT state so a halted CPU resumes, matching real HW.
void emulate_interrupt(State8080 *state, int interrupt_num);
