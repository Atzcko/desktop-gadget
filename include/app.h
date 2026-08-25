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
