// ============================================================================
// display_ui.cpp
//  The lock screen plus the slide-up music drawer
// ============================================================================

#include <lvgl.h>
#include <stdint.h>
#include "audio_player.h"
#include "display_ui.h"

static constexpr lv_coord_t SCREEN_WIDTH = 320;
static constexpr lv_coord_t SCREEN_HEIGHT = 480;
static constexpr lv_coord_t DRAWER_OPEN_Y = 58;
static constexpr lv_coord_t DRAWER_CLOSED_Y = SCREEN_HEIGHT;

// Each value is a left-side drawer button. Tapping one travels through
// menu_row_event_cb() -> show_music_view() to update the right-side panel.
enum MusicView
{
    VIEW_PLAYLISTS,
    VIEW_SONGS,
    VIEW_COUNT
};

static lv_obj_t *time_label = nullptr;
static lv_obj_t *date_label = nullptr;
static lv_obj_t *lockscreen_background = nullptr;
static lv_obj_t *wallpaper_message_label = nullptr;
// These live on the home/lock screen now. main.cpp updates song/status, and
// pause_button_event_cb() asks audio_player.cpp to pause or resume playback.
static lv_obj_t *status_label = nullptr;
static lv_obj_t *song_label = nullptr;
static lv_obj_t *pause_button = nullptr;
static lv_obj_t *pause_button_label = nullptr;
static lv_obj_t *music_drawer = nullptr;
static lv_obj_t *menu_rows[VIEW_COUNT] = {};
static lv_obj_t *menu_labels[VIEW_COUNT] = {};
static lv_obj_t *menu_arrows[VIEW_COUNT] = {};
static lv_obj_t *panel_title = nullptr;
static lv_obj_t *panel_body = nullptr;
static MusicView active_view = VIEW_PLAYLISTS;
static bool drawer_open = false;

static void animate_drawer_to(lv_coord_t y)
{
    // Let the menu move like a phone drawer instead of abruptly teleporting in.
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, music_drawer);
    lv_anim_set_values(&anim, lv_obj_get_y(music_drawer), y);
    lv_anim_set_time(&anim, 230);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, [](void *obj, int32_t value) {
        lv_obj_set_y((lv_obj_t *)obj, (lv_coord_t)value);
    });
    lv_anim_start(&anim);
}

static void gesture_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_GESTURE)
    {
        return;
    }

    // LVGL reaches this function from lv_timer_handler() in main.cpp after
    // my_touchpad_read() has reported a swipe from the real touch controller.
    lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (direction == LV_DIR_TOP && !drawer_open)
    {
        drawer_open = true;
        animate_drawer_to(DRAWER_OPEN_Y);
    }
    else if (direction == LV_DIR_BOTTOM && drawer_open)
    {
        drawer_open = false;
        animate_drawer_to(DRAWER_CLOSED_Y);
    }
}

void display_ui_set_time(const char *time_text, const char *date_text)
{
    // clock_manager_update() in src/clock_manager.cpp prepares these strings from the
    // board's RTC; this function only knows how to put them on the labels.
    if (time_label)
    {
        lv_label_set_text(time_label, time_text);
    }
    if (date_label)
    {
        lv_label_set_text(date_label, date_text);
    }
}

void display_ui_set_status(const char *text)
{
    // music_controller.cpp uses this while loading/finishing a song, and the pause callback
    // below reuses it so every playback message lands in the same spot.
    if (status_label)
    {
        lv_label_set_text(status_label, text);
    }
}

void display_ui_set_song(const char *text)
{
    // music_controller_start() in src/music_controller.cpp hands us the path selected
    // by file_browser_find_nth_wav(), such as /music/second_song.wav.
    if (song_label)
    {
        lv_label_set_text(song_label, text);
    }
}

void display_ui_set_wallpaper_message(const char *text)
{
    if (!wallpaper_message_label)
    {
        return;
    }

    lv_label_set_text(wallpaper_message_label, text ? text : "");
    if (text && text[0] != '\0')
    {
        lv_obj_clear_flag(wallpaper_message_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(wallpaper_message_label, LV_OBJ_FLAG_HIDDEN);
    }
}

bool display_ui_set_background(const char *image_path)
{
    if (!lockscreen_background || !image_path || image_path[0] == '\0')
    {
        display_ui_set_wallpaper_message("Wallpaper path missing");
        return false;
    }

    // "S:" points at the SD card driver set up by
    // sd_manager_register_lvgl_filesystem() in src/sd_manager.cpp.
    // The picture is kept behind the clock and drawer, like actual wallpaper.
    static String lvgl_path;
    lvgl_path = String("S:") + image_path;

    // First ask LVGL for the dimensions without decoding every pixel. Your LCD
    // is 320 x 480; a huge phone/tablet PNG cannot be safely shrunk by this
    // widget. Next practical step is putting a 320 x 480 copy in SD /images.
    lv_img_header_t header;
    if (lv_img_decoder_get_info(lvgl_path.c_str(), &header) != LV_RES_OK)
    {
        display_ui_set_wallpaper_message("Wallpaper format failed");
        return false;
    }
    if (header.w > SCREEN_WIDTH || header.h > SCREEN_HEIGHT)
    {
        display_ui_set_wallpaper_message("Wallpaper too big\nuse 320 x 480");
        return false;
    }

    // Opening once here catches a real decode/memory failure. The pixel memory
    // comes from PSRAM through lv_conf.h; without that, even modest PNG files
    // can fail before LVGL has anything visible to draw.
    lv_img_decoder_dsc_t decoder;
    if (lv_img_decoder_open(&decoder, lvgl_path.c_str(), lv_color_black(), 0) != LV_RES_OK)
    {
        display_ui_set_wallpaper_message("Wallpaper decode failed\ntry JPG or BMP");
        return false;
    }
    lv_img_decoder_close(&decoder);

    lv_img_set_src(lockscreen_background, lvgl_path.c_str());
    lv_obj_center(lockscreen_background);
    lv_obj_move_background(lockscreen_background);
    display_ui_set_wallpaper_message("");
    return true;
}

void display_ui_set_pause_button_enabled(bool enabled)
{
    if (!pause_button)
    {
        return;
    }

    if (enabled)
    {
        lv_obj_clear_state(pause_button, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_add_state(pause_button, LV_STATE_DISABLED);
    }
}

static void pause_button_event_cb(lv_event_t *event)
{
    // This is linked from the button created near the bottom of
    // display_ui_create(). The sound-side state change happens next inside
    // audio_player_toggle_pause() in src/audio_player.cpp.
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !audio_player_toggle_pause())
    {
        return;
    }

    if (audio_player_is_paused())
    {
        lv_label_set_text(pause_button_label, LV_SYMBOL_PLAY);
        display_ui_set_status("Paused");
    }
    else
    {
        lv_label_set_text(pause_button_label, LV_SYMBOL_PAUSE);
        display_ui_set_status("Playing");
    }
}

static void show_music_view(MusicView view)
{
    static const char *titles[VIEW_COUNT] = {
        "PLAYLISTS", "SONGS"};
    static const char *descriptions[VIEW_COUNT] = {
        "Saved playlists\nfrom /music",
        "Songs loaded\nfrom SD card"};

    // menu_row_event_cb() passes in the enum belonging to the row that was
    // touched; active_view stores which blue highlight is currently selected.
    active_view = view;
    for (int i = 0; i < VIEW_COUNT; i++)
    {
        bool selected = i == (int)view;
        lv_obj_set_style_bg_color(menu_rows[i],
                                  selected ? lv_color_hex(0x3276D6) : lv_color_hex(0x000000),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(menu_rows[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_color(menu_labels[i],
                                    selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xD6DFEA),
                                    LV_PART_MAIN);
        if (selected)
        {
            lv_obj_clear_flag(menu_arrows[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(menu_arrows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_label_set_text(panel_title, titles[view]);
    // Each library section loads into this same neat little right pane.
    // The actual playing song now stays on the lockscreen where it belongs.
    lv_label_set_text(panel_body, descriptions[view]);
}

static void menu_row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
        // make_menu_row() stashed the row's MusicView in user_data below.
        // Pull it back out here and let show_music_view() redraw the panel.
        show_music_view((MusicView)(intptr_t)lv_event_get_user_data(event));
    }
}

static lv_obj_t *make_menu_row(lv_obj_t *parent, const char *text, lv_coord_t y, MusicView view)
{
    lv_obj_t *row = lv_btn_create(parent);
    lv_obj_set_size(row, 126, 35);
    lv_obj_set_pos(row, 0, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, menu_row_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)view);
    lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xD6DFEA), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 9, 0);

    lv_obj_t *arrow = lv_label_create(row);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -7, 0);
    lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);

    menu_rows[view] = row;
    menu_labels[view] = label;
    menu_arrows[view] = arrow;
    return row;
}

static void build_music_drawer(void)
{
    // This whole object starts just below the screen. gesture_event_cb()
    // animates it upward when you swipe on the screen created below.
    music_drawer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(music_drawer, SCREEN_WIDTH, SCREEN_HEIGHT - DRAWER_OPEN_Y);
    lv_obj_set_pos(music_drawer, 0, DRAWER_CLOSED_Y);
    lv_obj_clear_flag(music_drawer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(music_drawer, gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_set_style_radius(music_drawer, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(music_drawer, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(music_drawer, lv_color_hex(0x4A5666), LV_PART_MAIN);
    lv_obj_set_style_bg_color(music_drawer, lv_color_hex(0x05080E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(music_drawer, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_all(music_drawer, 0, LV_PART_MAIN);

    lv_obj_t *handle = lv_obj_create(music_drawer);
    lv_obj_set_size(handle, 44, 4);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_radius(handle, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(handle, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xB7C3D1), LV_PART_MAIN);

    lv_obj_t *bar = lv_obj_create(music_drawer);
    lv_obj_set_size(bar, SCREEN_WIDTH - 2, 37);
    lv_obj_set_pos(bar, 1, 19);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x161D26), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_70, LV_PART_MAIN);

    lv_obj_t *bar_title = lv_label_create(bar);
    lv_label_set_text(bar_title, "Music");
    lv_obj_set_style_text_color(bar_title, lv_color_hex(0xD7E0EA), LV_PART_MAIN);
    lv_obj_align(bar_title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *battery = lv_obj_create(bar);
    lv_obj_set_size(battery, 25, 12);
    lv_obj_align(battery, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_radius(battery, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(battery, lv_color_hex(0xAFBAC7), LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery, lv_color_hex(0xA6D16C), LV_PART_MAIN);

    lv_obj_t *menu = lv_obj_create(music_drawer);
    lv_obj_set_size(menu, 127, 310);
    lv_obj_set_pos(menu, 1, 57);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu, LV_OPA_TRANSP, LV_PART_MAIN);

    make_menu_row(menu, "Playlists", 0, VIEW_PLAYLISTS);
    make_menu_row(menu, "Songs", 35, VIEW_SONGS);

    lv_obj_t *content_panel = lv_obj_create(music_drawer);
    lv_obj_set_size(content_panel, 191, 310);
    lv_obj_set_pos(content_panel, 128, 57);
    lv_obj_clear_flag(content_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(content_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(content_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(content_panel, LV_OPA_TRANSP, LV_PART_MAIN);

    panel_title = lv_label_create(content_panel);
    lv_label_set_text(panel_title, "PLAYLISTS");
    lv_obj_set_style_text_color(panel_title, lv_color_hex(0xA6BAD0), LV_PART_MAIN);
    lv_obj_align(panel_title, LV_ALIGN_TOP_LEFT, 2, 12);

    panel_body = lv_label_create(content_panel);
    lv_label_set_text(panel_body, "");
    lv_obj_set_style_text_color(panel_body, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(panel_body, LV_ALIGN_TOP_LEFT, 2, 48);

    show_music_view(VIEW_PLAYLISTS);
}

void display_ui_create(void)
{
    // setup() in main.cpp calls this before trying the SD card or starting
    // music, so the status setters already have labels ready to update.
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x080D14), LV_PART_MAIN);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(lv_scr_act(), gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // Empty until SD is mounted. Later the route is:
    // music_controller.cpp -> file_browser_find_background_image() -> display_ui_set_background().
    lockscreen_background = lv_img_create(lv_scr_act());
    lv_obj_center(lockscreen_background);

    // Hidden unless wallpaper loading fails. This is intentionally on-screen
    // because you asked to avoid depending on Serial Monitor for debugging.
    wallpaper_message_label = lv_label_create(lv_scr_act());
    lv_label_set_text(wallpaper_message_label, "");
    lv_obj_set_width(wallpaper_message_label, 270);
    lv_obj_set_style_text_align(wallpaper_message_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(wallpaper_message_label, lv_color_hex(0xFFDF89), LV_PART_MAIN);
    lv_obj_align(wallpaper_message_label, LV_ALIGN_TOP_MID, 0, 196);
    lv_obj_add_flag(wallpaper_message_label, LV_OBJ_FLAG_HIDDEN);

    date_label = lv_label_create(lv_scr_act());
    lv_label_set_text(date_label, "SUNDAY, MAY 24");
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xA8B7C8), LV_PART_MAIN);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 108);

    time_label = lv_label_create(lv_scr_act());
    lv_label_set_text(time_label, "--:--");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 128);

    // Now-playing lives on the lockscreen, so you can glance at the song and
    // pause it without opening the music library drawer first.
    song_label = lv_label_create(lv_scr_act());
    lv_label_set_text(song_label, "/music");
    lv_label_set_long_mode(song_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(song_label, 276);
    lv_obj_set_style_text_align(song_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(song_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(song_label, LV_ALIGN_BOTTOM_MID, 0, -142);

    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "Looking for music...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xD2DCE7), LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -116);

    pause_button = lv_btn_create(lv_scr_act());
    lv_obj_set_size(pause_button, 62, 62);
    lv_obj_align(pause_button, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_add_event_cb(pause_button, pause_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(pause_button, LV_STATE_DISABLED);
    lv_obj_set_style_radius(pause_button, 31, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pause_button, lv_color_hex(0x3478D2), LV_PART_MAIN);

    pause_button_label = lv_label_create(pause_button);
    lv_label_set_text(pause_button_label, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_color(pause_button_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(pause_button_label);

    lv_obj_t *handle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(handle, 48, 5);
    lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_radius(handle, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(handle, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(handle, lv_color_hex(0xBBC5D1), LV_PART_MAIN);

    build_music_drawer();
}
