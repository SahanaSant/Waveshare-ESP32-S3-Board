#pragma once

// setup() in src/main.cpp builds the widgets once; the setters below update
// those same saved widget pointers while music and clock code keep running.
void display_ui_create(void);

// update_lock_screen_clock() in src/main.cpp calls this twice a second.
void display_ui_set_time(const char *time_text, const char *date_text);

// Playback startup/end and pause taps all report what is happening through this.
void display_ui_set_status(const char *text);
void display_ui_set_song(const char *text);

// The path comes from file_browser_find_background_image() after
// sd_manager_register_lvgl_filesystem() has made the SD card visible to LVGL.
void display_ui_set_background(const char *image_path);

// Enabled once audio_player_start_wav() succeeds, disabled when the song ends.
void display_ui_set_pause_button_enabled(bool enabled);
