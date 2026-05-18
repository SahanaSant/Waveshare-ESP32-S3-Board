// ============================================================================
// display_ui.cpp
//  LVGL screens, button, labels, progress bar
// ============================================================================

#include <lvgl.h>

// ============================================================================
//  UI STATE
//  Kept private to the display module so main.cpp only coordinates modules
// ============================================================================
static int button_press_count = 0;
static lv_obj_t *status_label = nullptr;

// ============================================================================
//  UI EVENTS
//  Callback functions that define what widgets do when the user interacts
// ============================================================================
static void button_event_cb(lv_event_t *e)
{
    button_press_count++;
    lv_label_set_text_fmt(status_label, "button pressed: %d times", button_press_count);
}

// ============================================================================
//  UI CONSTRUCTION
//  Build and arrange the visible LVGL widgets for this screen
// ============================================================================
void display_ui_create(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101820), LV_PART_MAIN);

    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(status_label, "button pressed: %d times", button_press_count);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *button = lv_btn_create(lv_scr_act());
    lv_obj_set_size(button, 180, 70);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "Tap me");
    lv_obj_center(button_label);

    lv_obj_t *button2 = lv_btn_create(lv_scr_act());
    lv_obj_set_size(button2, 180, 70);
    lv_obj_align(button2, LV_ALIGN_CENTER, 0, 90);
    lv_obj_add_event_cb(button2, button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *button_label2 = lv_label_create(button2);
    lv_label_set_text(button_label2, "Tap me as well");
    lv_obj_center(button_label2);
}
