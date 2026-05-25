// ============================================================================
// visualizer.cpp
//  Lightweight centre-stage bars, ready for real audio levels later
// ============================================================================
//
// These bars are currently styled placeholders, not sound-reactive FFT bars.
// Updating thirteen LVGL objects many times per second would redraw a large
// wallpaper-covered part of the screen. A future live version should first
// expose cheap audio level buckets and use a controlled frame rate.

#include <lvgl.h>
#include "accent_colors.h"
#include "visualizer.h"

static constexpr int BAR_COUNT = 13;
static lv_obj_t *bars[BAR_COUNT] = {};

void visualizer_apply_accent_colors(void)
{
    // Repeating the four palette roles distributes the wallpaper theme across
    // the stage rather than painting one flat stripe of the dominant color.
    const uint32_t bar_colours[BAR_COUNT] = {
        wallpaper_accents.muted,     wallpaper_accents.primary,   wallpaper_accents.secondary,
        wallpaper_accents.primary,   wallpaper_accents.highlight, wallpaper_accents.secondary,
        wallpaper_accents.primary,   wallpaper_accents.highlight, wallpaper_accents.secondary,
        wallpaper_accents.primary,   wallpaper_accents.secondary, wallpaper_accents.primary,
        wallpaper_accents.muted};

    for (int i = 0; i < BAR_COUNT; ++i)
    {
        if (bars[i])
        {
            lv_obj_set_style_bg_color(bars[i], lv_color_hex(bar_colours[i]), LV_PART_MAIN);
        }
    }
}

void visualizer_create(lv_obj_t *parent)
{
    // For now these are resting bars, not pretend FFT output: they show the
    // wallpaper-based colour language without scheduling constant redraws
    // while a song plays. Later audio_player.cpp can expose level buckets and
    // a visualizer_update() function here can change only these heights.
    static const lv_coord_t resting_heights[BAR_COUNT] = {
        28, 50, 73, 42, 94, 62, 112, 78, 96, 45, 70, 52, 30};
    lv_obj_t *stage = lv_obj_create(parent);
    // This is a positioning parent only: transparent means the user's
    // wallpaper remains visible behind every colored bar.
    lv_obj_set_size(stage, 274, 210);
    lv_obj_align(stage, LV_ALIGN_CENTER, 0, -6);
    lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(stage, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_pad_all(stage, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(stage, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(stage, LV_OPA_TRANSP, LV_PART_MAIN);

    for (int i = 0; i < BAR_COUNT; ++i)
    {
        lv_obj_t *bar = lv_obj_create(stage);
        bars[i] = bar;
        lv_obj_set_size(bar, 12, resting_heights[i]);
        lv_obj_set_pos(bar, 12 + (i * 19), 190 - resting_heights[i]);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        // A swipe may begin on a coloured bar. Let that gesture travel next
        // to gesture_event_cb() in display_ui.cpp so the drawer still opens.
        lv_obj_add_flag(bar, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_80, LV_PART_MAIN);
    }

    // This initially paints fallback colours; once display_ui_set_background()
    // samples the PNG it calls this function again with your image's colours.
    visualizer_apply_accent_colors();
}
