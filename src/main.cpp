// ============================================================================
//  main.cpp 
//  coordinates everything
// ============================================================================

// ============================================================================
//  LIBRARIES
//  Display driver, UI framework, GPIO expander, and capacitive touch driver
// ============================================================================
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "TCA9554.h"
#include "TouchDrvFT6X36.hpp"
#include "audio_player.h"
#include "display_ui.h"
#include "file_browser.h"
#include "sd_manager.h"

// ============================================================================
//  BOARD PIN MAP
//  Physical connections used by the Waveshare ESP32-S3-Touch-LCD-3.5 board
// ============================================================================
#define GFX_BL 6

#define SPI_MISO 2
#define SPI_MOSI 1
#define SPI_SCLK 5

#define LCD_CS -1
#define LCD_DC 3
#define LCD_RST -1
#define LCD_HOR_RES 320
#define LCD_VER_RES 480

#define I2C_SDA 8
#define I2C_SCL 7

// ============================================================================
//  HARDWARE OBJECTS
//  Low-level devices that let the ESP32 talk to the screen and touch panel
// ============================================================================
TCA9554 TCA(0x20);
TouchDrvFT6X36 touch;

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC /* DC */, LCD_CS /* CS */, SPI_SCLK /* SCK */, SPI_MOSI /* MOSI */, SPI_MISO /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7796(
    bus, LCD_RST /* RST */, 0 /* rotation */, true /* IPS */, LCD_HOR_RES, LCD_VER_RES);
 
// ============================================================================
//  LVGL STATE
//  Buffers and display driver state owned by the main coordinator
// ============================================================================

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_disp_draw_buf_t draw_buf;
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;
lv_disp_drv_t disp_drv;

static bool start_sd_audio_playback(void)
{
    // This function is the mini playlist flow:
    // 1. Mount SD
    // 2. Find the second WAV in /music
    // 3. Tell the audio player to start it
    // 4. Keep the touchscreen updated instead of using Serial Monitor
    display_ui_set_status("Mounting SD card...");

    if (!sd_manager_mount())
    {
        display_ui_set_status("SD card mount failed");
        return false;
    }

    display_ui_set_status("Scanning /music...");

    // 1 means "second song" because the finder counts like arrays:
    // 0 = first, 1 = second.
    String song_path = file_browser_find_nth_wav("/music", 1);
    if (song_path.length() == 0)
    {
        display_ui_set_status("Need 2 WAVs in /music");
        return false;
    }

    display_ui_set_song(song_path.c_str());
    display_ui_set_status("Opening WAV...");

    if (!audio_player_start_wav(song_path))
    {
        display_ui_set_status(audio_player_last_error());
        return false;
    }

    display_ui_set_status("Playing");
    return true;
}

// ============================================================================
//  DISPLAY HARDWARE HELPERS
//  Panel reset and the bridge from LVGL pixels to the physical LCD
// ============================================================================
void lcd_reset(void)
{
    TCA.write1(1, 1);
    delay(10);
    TCA.write1(1, 0);
    delay(10);
    TCA.write1(1, 1);
    delay(200);
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp);
}

// ============================================================================
//  TOUCH INPUT
//  Reads the capacitive touch controller and hands coordinates to LVGL
// ============================================================================
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    LV_UNUSED(indev_drv);
    int16_t x[1], y[1];
    uint8_t touched = touch.getPoint(x, y, 1);

    if (touched)
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x[0];
        data->point.y = y[0];
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}


// ============================================================================
//  STARTUP
//  Bring up the board, initialize LVGL, register drivers, and create the UI
// ============================================================================
void setup(void)
{
    Wire.begin(I2C_SDA, I2C_SCL);

    TCA.begin();
    TCA.pinMode1(1, OUTPUT);
    lcd_reset();

    if (!touch.begin(Wire, FT6X36_SLAVE_ADDRESS))
    {
        while (1)
        {
            delay(1000);
        }
    }

    gfx->begin();
    gfx->fillScreen(BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    lv_init();

    screenWidth = gfx->width();
    screenHeight = gfx->height();
    bufSize = screenWidth * 120;

    disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT);
    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT);

    if (!disp_draw_buf1 || !disp_draw_buf2)
    {
        while (1)
        {
            delay(1000);
        }
    }

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, bufSize);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    display_ui_create();

    if (!start_sd_audio_playback())
    {
        display_ui_set_song("/music");
    }
}

// ============================================================================
//  MAIN LOOP
//  Let LVGL process rendering, animations, and input continuously
// ============================================================================
void loop(void)
{
    // Keep feeding audio chunks. If this does not run often, the song will stutter.
    audio_player_loop();

    // When the player reaches the end, it exposes "Finished" as its status.
    // Main owns the screen, so main is the one that paints that message.
    if (strcmp(audio_player_last_error(), "Finished") == 0)
    {
        display_ui_set_status("Finished");
    }
    lv_timer_handler(); //lvgl will simply handle all the event callbacls
    //above the display ui image just produces a picture. that is stored in static storage
    delay(5);
    //heap handles lvgl draw buffers, lvgl widgets
}
