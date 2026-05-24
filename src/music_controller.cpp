// ============================================================================
// music_controller.cpp
//  Connects SD files, lockscreen wallpaper, WAV playback, and screen messages
// ============================================================================

#include <Arduino.h>
#include <cstring>
#include "audio_player.h"
#include "display_ui.h"
#include "file_browser.h"
#include "music_controller.h"
#include "sd_manager.h"

void music_controller_start(void)
{
    // Here is the full startup trail:
    // sd_manager.cpp mounts the card and registers image reads for LVGL.
    // file_browser.cpp chooses the wallpaper and second WAV path.
    // display_ui.cpp paints the background/song/status.
    // audio_player.cpp turns the WAV bytes into speaker audio.
    display_ui_set_status("Mounting SD card...");
    if (!sd_manager_mount())
    {
        display_ui_set_status("SD card mount failed");
        display_ui_set_song("/music");
        return;
    }

    // Next linked function: display_ui_set_background() validates the found
    // picture size and displays a helpful warning if it cannot become wallpaper.
    sd_manager_register_lvgl_filesystem();
    String background_path = file_browser_find_background_image("/images");
    if (background_path.length() > 0)
    {
        display_ui_set_background(background_path.c_str());
    }
    else
    {
        display_ui_set_wallpaper_message("No image in /images");
    }

    display_ui_set_status("Scanning /music...");

    // Index 1 means your second WAV: index 0 is the first file, index 1 the second.
    String song_path = file_browser_find_nth_wav("/music", 1);
    if (song_path.length() == 0)
    {
        display_ui_set_status("Need 2 WAVs in /music");
        display_ui_set_song("/music");
        return;
    }

    display_ui_set_song(song_path.c_str());
    display_ui_set_status("Opening WAV...");

    // Follow into src/audio_player.cpp next: it parses WAV details and sets
    // up the ES8311/I2S connection that actually drives the speaker.
    if (!audio_player_start_wav(song_path))
    {
        display_ui_set_status(audio_player_last_error());
        return;
    }

    display_ui_set_pause_button_enabled(true);
    display_ui_set_status("Playing");
}

void music_controller_update(void)
{
    // Follow this into audio_player_loop(): each pass sends another PCM block
    // toward the speaker without blocking touchscreen handling in main.cpp.
    audio_player_loop();

    if (strcmp(audio_player_last_error(), "Finished") == 0)
    {
        display_ui_set_status("Finished");
        display_ui_set_pause_button_enabled(false);
    }
}
