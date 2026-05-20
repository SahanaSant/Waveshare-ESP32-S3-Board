// ============================================================================
// file_browser.cpp
//  logic for parsing through SD card and fetching song
// ============================================================================

#include <SD_MMC.h>
#include "file_browser.h"

static bool is_wav_file(const String &path)
{
    // Make the extension check chill about caps, so SONG.WAV and song.wav both count.
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".wav");
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
    return find_nth_wav_in_dir(dir_path, target_index, current_index);
}
