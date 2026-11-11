#pragma once
#include <string>

// Resolve a project-relative path (e.g. "roms/invaders.h") by trying the
// working directory, its parent, and the executable's directory and its
// parents, so the game runs from the project root, build/, or bin/.
// Returns `relative` unchanged if nothing exists; the caller reports it.
std::string resolve_asset(const std::string &relative);
