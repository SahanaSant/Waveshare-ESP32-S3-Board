#pragma once

// Called first from music_controller_start() in src/music_controller.cpp.
// Nothing can search /music or /images until this says the SD card is ready.
bool sd_manager_mount(void);

// Called by src/music_controller.cpp after mounting succeeds. This gives LVGL the "S:"
// drive used later by display_ui_set_background() in src/display_ui.cpp.
void sd_manager_register_lvgl_filesystem(void);
