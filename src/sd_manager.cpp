// ============================================================================
// sd_manager.cpp
//  SD card mounts, file listing, opening files
// ============================================================================

#include <SD_MMC.h>
#include <lvgl.h>
#include "sd_manager.h"

#define SD_MMC_CLK 11
#define SD_MMC_CMD 10
#define SD_MMC_D0 9

// This file has two related jobs:
// 1. sd_manager_mount() turns the physical card into paths like /music/song.wav.
// 2. sd_manager_register_lvgl_filesystem() gives LVGL an "S:" version of those
//    same paths, so display_ui_set_background() can draw /images/lockscreen.jpg.
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

static void *open_lvgl_file(lv_fs_drv_t *driver, const char *path, lv_fs_mode_t mode)
{
    LV_UNUSED(driver);

    // Background art is read-only. Keeping that rule here avoids the UI ever
    // accidentally scribbling over stuff on your SD card.
    if (mode != LV_FS_MODE_RD)
    {
        return nullptr;
    }

    // display_ui.cpp gives LVGL "S:/images/lockscreen.jpg". LVGL removes the
    // "S:" and hands this callback the remaining path, which SD_MMC understands.
    String full_path = path[0] == '/' ? String(path) : String("/") + path;
    File *file = new File(SD_MMC.open(full_path, FILE_READ));
    if (!*file)
    {
        delete file;
        return nullptr;
    }
    return file;
}

static lv_fs_res_t close_lvgl_file(lv_fs_drv_t *driver, void *file_ptr)
{
    LV_UNUSED(driver);
    File *file = static_cast<File *>(file_ptr);
    file->close();
    delete file;
    return LV_FS_RES_OK;
}

static lv_fs_res_t read_lvgl_file(lv_fs_drv_t *driver, void *file_ptr, void *buffer,
                                  uint32_t bytes_to_read, uint32_t *bytes_read)
{
    LV_UNUSED(driver);
    File *file = static_cast<File *>(file_ptr);
    // The PNG/BMP/JPG decoder in LVGL calls this repeatedly while it draws the
    // wallpaper widget created by display_ui_create().
    *bytes_read = file->read(static_cast<uint8_t *>(buffer), bytes_to_read);
    return LV_FS_RES_OK;
}

static lv_fs_res_t seek_lvgl_file(lv_fs_drv_t *driver, void *file_ptr, uint32_t position,
                                  lv_fs_whence_t origin)
{
    LV_UNUSED(driver);
    File *file = static_cast<File *>(file_ptr);
    uint32_t target = position;

    if (origin == LV_FS_SEEK_CUR)
    {
        target = file->position() + position;
    }
    else if (origin == LV_FS_SEEK_END)
    {
        target = file->size() + position;
    }

    return file->seek(target) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t tell_lvgl_file(lv_fs_drv_t *driver, void *file_ptr, uint32_t *position)
{
    LV_UNUSED(driver);
    File *file = static_cast<File *>(file_ptr);
    *position = file->position();
    return LV_FS_RES_OK;
}

void sd_manager_register_lvgl_filesystem(void)
{
    static bool registered = false;
    if (registered)
    {
        return;
    }

    // LVGL sees paths beginning with S:, and these callbacks hand those reads
    // over to the very same SD card that holds the songs. After this finishes,
    // music_controller.cpp can safely call display_ui_set_background("/images/...").
    static lv_fs_drv_t sd_driver;
    lv_fs_drv_init(&sd_driver);
    sd_driver.letter = 'S';
    sd_driver.cache_size = 1024;
    sd_driver.open_cb = open_lvgl_file;
    sd_driver.close_cb = close_lvgl_file;
    sd_driver.read_cb = read_lvgl_file;
    sd_driver.seek_cb = seek_lvgl_file;
    sd_driver.tell_cb = tell_lvgl_file;
    lv_fs_drv_register(&sd_driver);
    registered = true;
}
