/**
 * ui.h — the clock screen.
 *
 * Only the LVGL task may call any of this.
 */
#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <time.h>

void      ui_init(uint16_t screen_w, uint16_t screen_h);
lv_obj_t *ui_screen(void);

/* Set the time. Animates a split-flap fold on any card whose rendered
 * digits differ from the requested ones; renders instantly if `animate`
 * is false (first paint, and returning from Settings). */
void ui_set_time(int hour, int minute, bool animate);

void ui_set_weather(float current, float lo, float hi, float humidity,
                    int code, bool is_day, bool valid, bool stale);
void ui_show_weather_block(bool visible);
void ui_show_humidity(bool visible);

/* Battery chip, top right. Hidden when no battery is connected, and while
 * the line display owns that corner. */
void ui_set_battery(bool present, int pct, bool charging);

/* Anti burn-in: nudge the whole layout. */
void ui_set_offset(int dx, int dy);

/* Tap overlay: date + how long ago weather last synced. */
void ui_show_info(const char *date_line, const char *sync_line);

/* Emotion overlay. Called only from emotion_tick(), i.e. the LVGL task. */
void ui_emotion_show(uint8_t state, const char *message);
void ui_emotion_clear(void);
