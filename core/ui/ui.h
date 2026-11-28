// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The user interface task (docs/ux/, docs/spec/ui.md)
#ifndef UI_H
#define UI_H

#include "ui_app.h"

void ui_start(void);
// The screen shown now, for the debug link's view: "home", "quick", "hub", "find", "diag", "hotspot", ... (any task)
const char *ui_screen_name(void);
// What is on screen (screen, depth, focus, banner, lock, Find query), as of the UI task's last pass (any task)
void ui_get_status(ui_app_status_t *st);
// The backlight level last set, percent (any task)
uint8_t ui_backlight(void);
// Settings > Display > Screen-off flash: the LED beacon while the screen is off (any task), its color
// (0 green, 1 red, 2 green and red) and its period in ms
bool ui_standby_led(void);
uint8_t ui_standby_led_color(void);
uint16_t ui_standby_led_ms(void);

#endif
