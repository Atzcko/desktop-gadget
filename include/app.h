/**
 * app.h — the few things the UI needs from main.cpp.
 *
 * Keeps ui.cpp and ui_settings.cpp free of any dependency on the panel
 * driver or the clock, so they only ever talk LVGL.
 */
#pragma once
#include <stdint.h>

/*
 * Battery, as read from the SY6970. The PMU has no fuel gauge — no coulomb
 * counter — so pct is a piecewise estimate from the cell voltage (D042).
 * pct is -1 when no battery is connected.
 */
struct BatteryState {
    bool     present;
    bool     charging;
    bool     vbus;
    uint16_t mv;
    int      pct;
};
BatteryState app_battery(void);

void app_apply_brightness(uint8_t level);   /* live panel brightness       */
void app_show_info_overlay(void);           /* tap: date + weather sync age */
void app_refresh_clock(bool animate);       /* repaint from the RTC        */
void app_apply_rotation(uint8_t rotation);  /* boot-time: panel only (0-3) */

/*
 * Set the panel + LVGL display resolution, nothing else — no settings write,
 * no clock rebuild. The host uses it to force landscape under apps that are
 * laid out for 600x450 and to restore the owner's orientation after (D043).
 */
void app_panel_rotate(uint8_t rotation);

/* Full live change: home, rotate, rebuild the clock for the new shape.
 * Callable only from the LVGL task. */
void app_apply_rotation_live(uint8_t rotation);

/* Thread-safe request from other tasks (HTTP); served on the LVGL loop. */
void app_request_rotation(uint8_t rotation);

/* Rebuild everything visible at the CURRENT rotation — a theme change needs
 * the rebuild without the rotate, and the rotate path skips no-ops (D049). */
void app_request_rebuild(void);

/*
 * Raw touch-point COUNT from the controller, for the trackpad's two-finger
 * detection (D051) — LVGL's indev only ever reports the first finger. Reads
 * the same CST226 state the indev polls; count-only, positions untouched.
 */
uint8_t app_touch_count(void);

/* Both touch points, for the Game Boy's gamepad zones (D056): D-pad and a
 * button pressed TOGETHER is two fingers, which LVGL's single-point indev
 * can never report. Returns the count; fills up to n points. */
uint8_t app_touch_points(int16_t *xs, int16_t *ys, uint8_t n);
