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
#include "SensorPCF85063.hpp"
#include "audio_player.h"
#include "display_ui.h"
#include "file_browser.h"
#include "sd_manager.h"

// The app path is intentionally split up so each file has one job:
// sd_manager.cpp gets files off the card, file_browser.cpp chooses paths,
// audio_player.cpp turns a WAV into sound, and display_ui.cpp draws feedback.
// The function that connects all four pieces is start_sd_audio_playback() below.

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
SensorPCF85063 rtc;
// update_lock_screen_clock() checks this before asking the RTC for real time,
// then forwards printable text into display_ui_set_time() in display_ui.cpp.
static bool rtc_ready = false;

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

static void init_lock_screen_clock(void)
{
    // There is a small clock chip on this board, so the time can keep ticking
    // even while the display is doing music things.
    rtc_ready = rtc.begin(Wire);
    if (!rtc_ready)
    {
        return;
    }

    RTC_DateTime datetime = rtc.getDateTime();
    if (datetime.getYear() < 2025)
    {
        // A dead/brand-new RTC has nonsense time. Seed it once from the time
        // this firmware was built, then it can carry on by itself.
        rtc.setDateTime(RTC_DateTime(__DATE__, __TIME__));
    }
}

static void update_lock_screen_clock(void)
{
    static uint32_t last_update_ms = 0;
    static const char *month_names[] = {
        "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
        "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
    char time_text[8];
    char date_text[24];

    if (millis() - last_update_ms < 500)
    {
        return;
    }
    last_update_ms = millis();

    if (!rtc_ready)
    {
        display_ui_set_time("--:--", "CLOCK NOT FOUND");
        return;
    }

    RTC_DateTime datetime = rtc.getDateTime();
    uint8_t hour = datetime.getHour();
    uint8_t display_hour = hour % 12;
    if (display_hour == 0)
    {
        display_hour = 12;
    }

    snprintf(time_text, sizeof(time_text), "%u:%02u", display_hour, datetime.getMinute());
    uint8_t month_index = (datetime.getMonth() >= 1 && datetime.getMonth() <= 12)
                              ? datetime.getMonth() - 1
                              : 0;
    snprintf(date_text, sizeof(date_text), "%s %u", month_names[month_index], datetime.getDay());
    display_ui_set_time(time_text, date_text);
}

static bool start_sd_audio_playback(void)
{
    // This function is the mini playlist flow:
    // 1. sd_manager_mount() unlocks access to the physical SD card.
    // 2. file_browser_find_background_image() finds /images/lockscreen.*,
    //    then display_ui_set_background() puts it behind the home screen.
    // 3. file_browser_find_nth_wav() locates the second WAV under /music.
    // 4. audio_player_start_wav() opens that file and gets the speaker going.
    // 5. Every failure/status is painted through display_ui.cpp instead of Serial.
    display_ui_set_status("Mounting SD card...");

    if (!sd_manager_mount())
    {
        display_ui_set_status("SD card mount failed");
        return false;
    }

    // The SD card is finally available, so let LVGL read wallpaper files too.
    // Follow this call into src/sd_manager.cpp: it registers the "S:" prefix
    // which display_ui_set_background() uses when creating an LVGL image source.
    sd_manager_register_lvgl_filesystem();

    // Follow this finder into src/file_browser.cpp: it prefers an image whose
    // filename contains "lockscreen", so album art cannot steal the wallpaper.
    String background_path = file_browser_find_background_image("/images");
    if (background_path.length() > 0)
    {
        // This updates lockscreen_background, created inside display_ui_create().
        display_ui_set_background(background_path.c_str());
    }

    display_ui_set_status("Scanning /music...");

    // 1 means "second song" because the finder counts like arrays:
    // 0 = first, 1 = second. In src/file_browser.cpp, the recursive helper
    // carries that count even if you make subfolders inside /music.
    String song_path = file_browser_find_nth_wav("/music", 1);
    if (song_path.length() == 0)
    {
        display_ui_set_status("Need 2 WAVs in /music");
        return false;
    }

    display_ui_set_song(song_path.c_str());
    display_ui_set_status("Opening WAV...");

    // Next stop for learning the audio flow: audio_player_start_wav() parses
    // the WAV header and configures the ES8311/I2S speaker connection.
    if (!audio_player_start_wav(song_path))
    {
        display_ui_set_status(audio_player_last_error());
        return false;
    }

    display_ui_set_pause_button_enabled(true);
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
    init_lock_screen_clock();

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

    // Create labels/buttons before start_sd_audio_playback() starts writing
    // status and song text into them. See display_ui_create() for layout.
    display_ui_create();
    update_lock_screen_clock();

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
    update_lock_screen_clock();

    // Keep feeding audio chunks. If this does not run often, the song will stutter.
    // See audio_player_loop(): it reads the next PCM bytes and writes them to I2S.
    audio_player_loop();

    // When the player reaches the end, it exposes "Finished" as its status.
    // Main owns the screen, so main is the one that paints that message.
    if (strcmp(audio_player_last_error(), "Finished") == 0)
    {
        display_ui_set_status("Finished");
        display_ui_set_pause_button_enabled(false);
    }
    // LVGL runs touchscreen callbacks here. For example, tapping pause jumps
    // to pause_button_event_cb() in src/display_ui.cpp.
    lv_timer_handler();
    delay(5);
}
