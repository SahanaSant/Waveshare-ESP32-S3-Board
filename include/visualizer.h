#pragma once

#include <lvgl.h>

// display_ui_create() calls this once after creating the wallpaper object.
// It builds the centre-stage bars using the shared palette in accent_colors.cpp.
// The bars rest for now; future live motion should update at a controlled rate.
void visualizer_create(lv_obj_t *parent);

// display_ui_set_background() calls this after accent_colors.cpp samples the
// loaded wallpaper; existing bars immediately take on the automatic palette.
void visualizer_apply_accent_colors(void);
