#pragma once

#include <Arduino.h>

// Public doorway into the sound engine. UI code does not touch SD handles,
// I2S, DMA buffers, FreeRTOS tasks, or the ES8311 directly; it calls these
// small actions and audio_player.cpp protects the timing-sensitive details.

// src/music_controller.cpp passes the WAV path found by file_browser_find_nth_wav() here.
// This parses its header, starts I2S/ES8311, and launches the private streaming
// task in src/audio_player.cpp so screen redraws cannot starve the song.
bool audio_player_start_wav(const String &path);

// The lockscreen pause button in display_ui.cpp calls these two together:
// toggle changes the state, is_paused chooses which icon/text to draw.
bool audio_player_toggle_pause(void);
bool audio_player_is_paused(void);

// The volume slider in display_ui.cpp sends a 0..100 value here. The ES8311
// codec applies it immediately and keeps it for the next song too.
void audio_player_set_volume(uint8_t volume);

// music_controller_update() samples this gently and passes it into the single
// progress bar in display_ui.cpp. 1000 means the complete WAV has played.
uint16_t audio_player_progress_per_mille(void);

// src/music_controller.cpp reads this while the private task plays the WAV,
// then paints failures or the final "Finished" state on the touchscreen.
const char *audio_player_last_error(void);
