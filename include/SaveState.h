#pragma once
#include <string>
#include "State8080.h"

// Save-state file: "SI80" magic, version byte, CPU registers/flags,
// interrupt + halt state, shift register, sound latches, 64K memory.
// Multi-byte fields are little-endian regardless of host.
//
// Live inputs (port1/port2) and the memory map are not stored: they belong
// to the running frontend, not the snapshot.
bool save_state(const State8080 &state, const std::string &path);

// Validates magic, version and exact size before touching `state`, so a
// truncated or foreign file leaves the running machine unchanged.
bool load_state(State8080 &state, const std::string &path);
