#pragma once

#include <Arduino.h>

// src/main.cpp passes the WAV path found by file_browser_find_nth_wav() here.
// This parses its header and starts the I2S/ES8311 hardware.
bool audio_player_start_wav(const String &path);

// loop() in src/main.cpp calls this repeatedly to keep audio bytes moving.
void audio_player_loop(void);

// The lockscreen pause button in display_ui.cpp calls these two together:
// toggle changes the state, is_paused chooses which icon/text to draw.
bool audio_player_toggle_pause(void);
bool audio_player_is_paused(void);

// src/main.cpp reads this to paint failures and the final "Finished" state.
const char *audio_player_last_error(void);
