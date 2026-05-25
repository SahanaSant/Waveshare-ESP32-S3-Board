#pragma once

// setup() in src/main.cpp calls this after Wire.begin() has opened the I2C bus.
// The implementation in src/clock_manager.cpp owns the RTC and updates display_ui.cpp.
// On a new flash it also repairs an RTC value older than the firmware build time.
void clock_manager_init(void);

// loop() in src/main.cpp calls this often; it only redraws the time twice a second.
// This keeps clock work tiny while audio playback runs on its separate task.
void clock_manager_update(void);
