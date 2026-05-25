#pragma once

#include <stdint.h>

// This is the small colour palette generated from the current SD wallpaper.
// display_ui_set_background() samples decoded PNG pixels before music starts,
// then display_ui.cpp and visualizer.cpp both paint with the result.
struct AccentColors
{
    // Primary: pause button and selected menu item.
    // Secondary: outlines, volume fill, and swipe handle.
    // Highlight: progress fill and volume knob for readable contrast.
    // Muted: dark support shade derived from primary for visualizer depth.
    uint32_t primary;
    uint32_t secondary;
    uint32_t highlight;
    uint32_t muted;
};

extern AccentColors wallpaper_accents;

// LVGL's PNG decoder provides RGB565 plus alpha bytes on this 16-bit display.
// display_ui.cpp sends that decoded startup image here before drawing playback.
void accent_colors_extract_from_rgb565_alpha(const uint8_t *pixels, uint32_t pixel_count);
