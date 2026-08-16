/**
 * ui.h — Fliqlo screen construction and update entry points.
 *
 * Only the LVGL task may call these. Background tasks (weather, HTTP)
 * post data through a queue; they never touch LVGL objects directly.
 */
#pragma once

#include <lvgl.h>
#include <stdbool.h>

/* Build the screen. Pass the panel dimensions from amoled.width()/height()
 * rather than literals — see D004, rotation 0 is already 600x450. */
void ui_init(uint16_t screen_w, uint16_t screen_h);

/* 24-hour clock. Digits are replaced immediately, no animation (Stage 2). */
void ui_set_time(int hour, int minute);

/* Weather block. When valid == false the last good values are kept on
 * screen and a stale indicator is shown instead (Stage 3). */
void ui_set_weather(float current, float lo, float hi, bool valid);
