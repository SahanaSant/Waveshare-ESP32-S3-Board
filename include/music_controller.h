#pragma once

// setup() in src/main.cpp calls this after display_ui_create(), because this
// startup flow immediately writes wallpaper/song status onto the screen.
void music_controller_start(void);

// loop() in src/main.cpp calls this to stream audio and report when it finishes.
void music_controller_update(void);
