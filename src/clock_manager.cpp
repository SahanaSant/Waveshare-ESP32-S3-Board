// ============================================================================
// clock_manager.cpp
//  The RTC side of the lockscreen, kept out of main so main stays readable
// ============================================================================

#include <Arduino.h>
#include <time.h>
#include <Wire.h>
#include "SensorPCF85063.hpp"
#include "clock_manager.h"
#include "display_ui.h"

static SensorPCF85063 rtc;
// clock_manager_update() checks this before asking the RTC for a time.
// Its next step is display_ui_set_time(), which changes the visible labels.
static bool rtc_ready = false;

// The PCF85063 is a real-time clock chip: after it is set once, its own small
// oscillator continues counting seconds separately from the ESP32 program.
// Important limitation: it does not magically know timezone/internet time.
// This project corrects stale values at flash time using the computer-local
// __DATE__/__TIME__ placed inside the firmware by the compiler.
static time_t local_epoch(RTC_DateTime datetime)
{
    // Both the chip time and __DATE__/__TIME__ are local clock readings, so
    // comparing them as local calendar values tells us if the saved RTC is
    // older than the firmware being flashed. tm_isdst = -1 lets C work out
    // daylight-saving time for the build date rather than assuming one.
    struct tm local_time = {};
    local_time.tm_year = datetime.getYear() - 1900;
    local_time.tm_mon = datetime.getMonth() - 1;
    local_time.tm_mday = datetime.getDay();
    local_time.tm_hour = datetime.getHour();
    local_time.tm_min = datetime.getMinute();
    local_time.tm_sec = datetime.getSecond();
    local_time.tm_isdst = -1;
    return mktime(&local_time);
}

void clock_manager_init(void)
{
    // Wire.begin() happens first in setup() in src/main.cpp. After that this
    // RTC can use the same I2C wires as the other small chips on the board.
    rtc_ready = rtc.begin(Wire);
    if (!rtc_ready)
    {
        return;
    }

    RTC_DateTime datetime = rtc.getDateTime();
    RTC_DateTime firmware_build_time(__DATE__, __TIME__);
    // Epoch seconds are only used for comparison. The value written to the
    // RTC below is still normal calendar date/time from firmware_build_time.
    time_t saved_time = local_epoch(datetime);
    time_t build_time = local_epoch(firmware_build_time);

    bool lost_power = !rtc.isClockIntegrityGuaranteed();
    bool missing_time = datetime.getYear() < 2025;
    // Two minutes avoids resetting a healthy clock merely because the upload
    // took a moment. If the saved chip is clearly behind a new firmware build,
    // this is the user's chance to repair it without opening Serial Monitor.
    bool behind_new_flash = saved_time < build_time - 120;

    if (lost_power || missing_time || behind_new_flash)
    {
        // Your clock previously accepted any believable old time, so values
        // like 1:24 stayed wrong forever. A newly uploaded firmware was built
        // from the computer's current local time; use it to catch the RTC up
        // once. On normal later reboots saved_time is newer than build_time,
        // so the battery-backed clock is left alone and continues naturally.
        //
        // Next linked code: clock_manager_update() reads the repaired RTC and
        // hands the top-bar labels to display_ui_set_time() in display_ui.cpp.
        rtc.setDateTime(firmware_build_time);
    }
}

void clock_manager_update(void)
{
    // Updating a clock label twice per second is enough for minute display.
    // It also avoids triggering unnecessary LVGL redraw regions while audio
    // and touch controls are busy.
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
    // RTC stores 24-hour values. The interface uses the familiar compact
    // 12-hour face, so midnight/noon must map from 0 to 12 deliberately.
    uint8_t display_hour = datetime.getHour() % 12;
    if (display_hour == 0)
    {
        display_hour = 12;
    }

    snprintf(time_text, sizeof(time_text), "%u:%02u", display_hour, datetime.getMinute());
    uint8_t month_index = (datetime.getMonth() >= 1 && datetime.getMonth() <= 12)
                              ? datetime.getMonth() - 1
                              : 0;
    snprintf(date_text, sizeof(date_text), "%s %u", month_names[month_index], datetime.getDay());

    // Follow this call into src/display_ui.cpp to see the two lockscreen
    // labels that receive these freshly formatted RTC strings.
    display_ui_set_time(time_text, date_text);
}
