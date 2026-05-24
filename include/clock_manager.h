#pragma once

// setup() in src/main.cpp calls this after Wire.begin() has opened the I2C bus.
// The implementation in src/clock_manager.cpp owns the RTC and updates display_ui.cpp.
void clock_manager_init(void);

// loop() in src/main.cpp calls this often; it only redraws the time twice a second.
void clock_manager_update(void);
