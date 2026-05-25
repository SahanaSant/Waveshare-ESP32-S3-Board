// ============================================================================
// file_browser.cpp
//  logic for parsing through SD card and fetching song
// ============================================================================

#include <SD_MMC.h>
#include "file_browser.h"

// This file only picks filenames. It never plays or draws anything itself:
// music_controller.cpp sends returned WAV paths to audio_player.cpp and returned image
// paths to display_ui.cpp.
// Keeping directory walking here avoids making the UI know how SD cards work
// or making the audio code decide what song "next" means.

static bool is_wav_file(const String &path)
{
    // Make the extension check chill about caps, so SONG.WAV and song.wav both count.
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".wav");
}

static bool is_background_image(const String &path)
{
    // LVGL uses its PNG/BMP/JPG decoders after this finder returns a path.
    // Next link: display_ui_set_background() turns that path into an LVGL image.
    // This means a normal phone-exported lockscreen.jpg works from /images now.
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".png") ||
           lower.endsWith(".bmp") ||
           lower.endsWith(".jpg");
}

static String find_nth_wav_in_dir(const char *dir_path, size_t target_index, size_t &current_index)
{
    // Open the folder we want to search. If it is missing or not actually a folder,
    // return an empty string and let main show the error.
    File dir = SD_MMC.open(dir_path);
    if (!dir || !dir.isDirectory())
    {
        return "";
    }

    // Walk through every item in this folder.
    // openNextFile() gives us one file/folder at a time until it runs out.
    File file = dir.openNextFile();
    while (file)
    {
        // `path` is kept as the complete SD path, not just the visible name,
        // because audio_player_start_wav() needs to reopen this exact item later.
        String path = file.path();
        if (file.isDirectory())
        {
            // If there are folders inside /music, search them too.
            // current_index is passed by reference so the count keeps going across folders.
            String nested = find_nth_wav_in_dir(path.c_str(), target_index, current_index);
            if (nested.length() > 0)
            {
                return nested;
            }
        }
        else if (is_wav_file(path))
        {
            // target_index is zero-based:
            // 0 = first WAV, 1 = second WAV, 2 = third WAV, etc.
            if (current_index == target_index)
            {
                return path;
            }

            // This was a WAV, just not the one we wanted, so bump the counter.
            current_index++;
        }

        file = dir.openNextFile();
    }

    return "";
}

String file_browser_find_nth_wav(const char *dir_path, size_t target_index)
{
    // Start counting from zero every time someone asks for a song.
    // The helper does the real searching because it needs to carry this count through recursion.
    size_t current_index = 0;
    // The resulting order is SD directory traversal order. If you later want
    // true alphabetical playlists, this is the module to collect and sort names.
    return find_nth_wav_in_dir(dir_path, target_index, current_index);
}

static String find_background_image_in_dir(const char *dir_path, String &fallback)
{
    // This mirrors the WAV search above, but it has one extra rule:
    // a name containing "lockscreen" wins over ordinary album/picture files.
    // Its returned path travels next to display_ui_set_background() through music_controller.cpp.
    File dir = SD_MMC.open(dir_path);
    if (!dir || !dir.isDirectory())
    {
        return "";
    }

    File file = dir.openNextFile();
    while (file)
    {
        String path = file.path();
        if (file.isDirectory())
        {
            String lockscreen = find_background_image_in_dir(path.c_str(), fallback);
            if (lockscreen.length() > 0)
            {
                return lockscreen;
            }
        }
        else if (is_background_image(path))
        {
            String lower = path;
            lower.toLowerCase();
            if (lower.indexOf("lockscreen") >= 0)
            {
                // A file named something like lockscreen.jpg is the obvious
                // choice even if album art is sitting in this folder too.
                return path;
            }
            if (fallback.length() == 0)
            {
                fallback = path;
            }
        }
        file = dir.openNextFile();
    }

    return "";
}

String file_browser_find_background_image(const char *dir_path)
{
    // The second value is our backup plan if /images has a picture but none
    // happens to be named lockscreen yet.
    String fallback;
    String lockscreen = find_background_image_in_dir(dir_path, fallback);
    // Background decoding/color sampling happens next in display_ui.cpp,
    // while no song is playing yet so the SD card is not being shared by audio.
    return lockscreen.length() > 0 ? lockscreen : fallback;
}
