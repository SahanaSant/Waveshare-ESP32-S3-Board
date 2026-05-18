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
#include <math.h>
#include "driver/i2s.h"
#include "TCA9554.h"
#include "TouchDrvFT6X36.hpp"
#include "../_official_3p5_demo/Arduino/libraries/es8311/es8311.h"
#include "../_official_3p5_demo/Arduino/libraries/es8311/es8311.cpp"

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
//  TEMPORARY SPEAKER TEST PINS
//  These match Waveshare's own audio examples for this board
// ============================================================================
#define I2S_MCLK 12
#define I2S_BCLK 13
#define I2S_LRCK 15
#define I2S_SDOUT 16

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_MCLK_HZ (AUDIO_SAMPLE_RATE * 256)
#define AUDIO_VOLUME 70

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

// ============================================================================
//  MODULE ENTRY POINTS
//  main.cpp coordinates these pieces without owning their internal details
// ============================================================================
void display_ui_create(void);

// ============================================================================
//  TEMPORARY SPEAKER TEST
//  Kept here on purpose for now; this can move into audio_player.cpp later
// ============================================================================
static bool init_speaker_test(void)
{
    es8311_handle_t es_handle = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
    if (!es_handle)
    {
        return false;
    }

    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = AUDIO_MCLK_HZ,
        .sample_frequency = AUDIO_SAMPLE_RATE};

    if (es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK)
    {
        return false;
    }

    es8311_voice_volume_set(es_handle, AUDIO_VOLUME, NULL);
    es8311_microphone_config(es_handle, false);

    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = AUDIO_SAMPLE_RATE, //44 100 audio samples per second 
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, //each sample is stored in a 16 bit numner 
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, 
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        //The dma basically creates a queue of memory, audio chunks are sent. DMA can allow the hardware to keep outputting audio while your CPU is doing other things
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = true, //use  a gud clock source
        .tx_desc_auto_clear = true,
        .fixed_mclk = AUDIO_MCLK_HZ, 
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_16BIT};

    const i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_MCLK,
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRCK,
        .data_out_num = I2S_SDOUT,
        .data_in_num = I2S_PIN_NO_CHANGE};

    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK)
    {
        return false;
    }

    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK)
    {
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    return true;
}

static void play_test_chime(void)
{
    constexpr size_t frame_count = 256;//create an array of 256 length
    int16_t samples[frame_count * 2];
    const float note_frequencies[] = {523.25f, 659.25f, 783.99f}; 

    for (float frequency : note_frequencies)
    {
        for (int chunk = 0; chunk < 60; chunk++) 
        {
            for (size_t i = 0; i < frame_count; i++)
            {
                const size_t sample_index = (chunk * frame_count) + i;
                const float phase = 2.0f * PI * frequency * sample_index / AUDIO_SAMPLE_RATE;
                const int16_t sample = (int16_t)(sinf(phase) * 5000); //create a sin wave to generate the sound 
                samples[i * 2] = sample; // samples make the chunk, the chunk makes the note
                samples[i * 2 + 1] = sample;
            }

            size_t bytes_written = 0;
            i2s_write(I2S_NUM_0, samples, sizeof(samples), &bytes_written, portMAX_DELAY); //send that chunk to the speaker, senk 60 chunks of each of those frequencies
        }
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
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

    if (init_speaker_test())
    {
        play_test_chime();
    }

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
}

// ============================================================================
//  MAIN LOOP
//  Let LVGL process rendering, animations, and input continuously
// ============================================================================
void loop(void)
{
    lv_timer_handler(); //lvgl will simply handle all the event callbacls
    //above the display ui image just produces a picture. that is stored in static storage
    delay(5);
    //heap handles lvgl draw buffers, lvgl widgets
}
