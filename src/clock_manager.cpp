// ============================================================================
// clock_manager.cpp
//  The RTC side of the lockscreen, kept out of main so main stays readable
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "SensorPCF85063.hpp"
#include "clock_manager.h"
#include "display_ui.h"

static SensorPCF85063 rtc;
// clock_manager_update() checks this before asking the RTC for a time.
// Its next step is display_ui_set_time(), which changes the visible labels.
static bool rtc_ready = false;

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
    if (datetime.getYear() < 2025)
    {
        // New/dead RTC chips start with nonsense time. Seed it from the date
        // the firmware was built, then this chip can keep time on its own.
        rtc.setDateTime(RTC_DateTime(__DATE__, __TIME__));
    }
}

void clock_manager_update(void)
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
