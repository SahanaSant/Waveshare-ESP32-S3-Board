// ============================================================================
// file_browser.cpp
//  logic for parsing through SD card and fetching song
// ============================================================================

#include <SD_MMC.h>
#include "file_browser.h"

static bool is_wav_file(const String &path)
{
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".wav");
}

static String find_nth_wav_in_dir(const char *dir_path, size_t target_index, size_t &current_index)
{
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
            String nested = find_nth_wav_in_dir(path.c_str(), target_index, current_index);
            if (nested.length() > 0)
            {
                return nested;
            }
        }
        else if (is_wav_file(path))
        {
            if (current_index == target_index)
            {
                return path;
            }
            current_index++;
        }

        file = dir.openNextFile();
    }

    return "";
}

String file_browser_find_nth_wav(const char *dir_path, size_t target_index)
{
    size_t current_index = 0;
    return find_nth_wav_in_dir(dir_path, target_index, current_index);
}
