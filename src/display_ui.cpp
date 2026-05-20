// ============================================================================
// display_ui.cpp
//  LVGL screens, button, labels, progress bar
// ============================================================================

#include <lvgl.h>

static lv_obj_t *status_label = nullptr;
static lv_obj_t *song_label = nullptr;

void display_ui_set_status(const char *text)
{
    if (status_label)
    {
        lv_label_set_text(status_label, text);
    }
}

void display_ui_set_song(const char *text)
{
    if (song_label)
    {
        lv_label_set_text(song_label, text);
    }
}

// ============================================================================
//  UI CONSTRUCTION
//  Build and arrange the visible LVGL widgets for this screen
// ============================================================================
void display_ui_create(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101820), LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(title_label, "WAV Player");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 42);

    song_label = lv_label_create(lv_scr_act());
    lv_label_set_text(song_label, "/music");
    lv_label_set_long_mode(song_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(song_label, 260);
    lv_obj_set_style_text_color(song_label, lv_color_hex(0xB8D8E8), LV_PART_MAIN);
    lv_obj_set_style_text_align(song_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(song_label, LV_ALIGN_CENTER, 0, -20);

    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "Looking for music...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 32);
}
