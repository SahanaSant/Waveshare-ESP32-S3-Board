// ============================================================================
// sd_manager.cpp
//  SD card mounts, file listing, opening files
// ============================================================================

#include <SD_MMC.h>
#include "sd_manager.h"

#define SD_MMC_CLK 11
#define SD_MMC_CMD 10
#define SD_MMC_D0 9

bool sd_manager_mount(void)
{
    // Tell Arduino exactly which pins the built-in SD slot uses.
    // The default ESP32 SD_MMC pins are wrong for this Waveshare board,
    // so skipping this line can make the board reboot or just never see the card.
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

    // Mount the card in 1-bit mode.
    // "/sdmmc" is just the internal mount name; the code still opens files like "/music/song.wav".
    // false = don't format the card if mounting fails, because that would be a very sad surprise.
    // 20000 = 20 MHz SD clock, matching the Waveshare example.
    return SD_MMC.begin("/sdmmc", true, false, 20000);
}
