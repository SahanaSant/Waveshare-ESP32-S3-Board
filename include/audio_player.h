#pragma once

#include <Arduino.h>

bool audio_player_start_wav(const String &path);
void audio_player_loop(void);
const char *audio_player_last_error(void);
