#pragma once
#include <cstdint>
#include <string>

// Disassemble the 8080 instruction at code[pc].
// Returns "0xNNNN MNEMONIC" as a string (no stdout side-effect).
// If out_len != nullptr, receives instruction length in bytes (1-3).
std::string disassemble8080_str(const uint8_t *code, int pc, int *out_len = nullptr);

// Legacy wrapper: prints to stdout, returns bytes consumed (1-3).
// Kept for compatibility; prefer disassemble8080_str for new code.
int disassemble8080(uint8_t *codebuffer, int pc);
