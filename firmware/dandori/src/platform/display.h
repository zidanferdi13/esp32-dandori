// Display, touch and backlight for the 7" board, exposed to LVGL 9.
#pragma once
#include <stdint.h>

// Initializes I2C, CH422G, RGB panel, GT911 and LVGL (display + pointer input).
bool displayBegin();
// Runs lv_timer_handler() and the screen-off timer. Call from loop().
void displayTick();

void displaySetBacklight(bool on);
bool displayBacklightOn();

// 0 = never. While the backlight is off, a touch only wakes the screen.
void displaySetScreenOffMinutes(uint16_t minutes);
