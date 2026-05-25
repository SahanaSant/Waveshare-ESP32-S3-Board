// ============================================================================
// input_manager.cpp
// touch/buttons, translating user input into commands
// ============================================================================

// This file is intentionally empty right now.
//
// Touch wiring and raw coordinate reads live in main.cpp because LVGL needs
// them as a display-driver callback. Button and gesture event handlers live in
// display_ui.cpp because they belong directly to the widgets they control.
//
// If controls later expand into hardware buttons, a wheel, headphones events,
// or reusable gesture commands, this is the natural module to own those input
// translations and call music_controller.cpp.
