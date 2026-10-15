#pragma once

// SDL_mixer frontend for the OUT3/OUT5 sound latches.
// All functions are non-fatal if samples are missing.
bool init_audio();
void shutdown_audio();
void poll_arcade_sounds();
