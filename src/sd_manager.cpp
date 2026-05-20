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
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    return SD_MMC.begin("/sdmmc", true, false, 20000);
}
