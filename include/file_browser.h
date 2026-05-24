#pragma once

#include <Arduino.h>

// src/main.cpp asks for index 1, which means the second WAV on the SD card.
// That returned path is handed straight to audio_player_start_wav().
String file_browser_find_nth_wav(const char *dir_path, size_t target_index);

// src/main.cpp uses this after mounting the SD card, then passes the returned
// /images path into display_ui_set_background() to make the wallpaper show.
String file_browser_find_background_image(const char *dir_path);
