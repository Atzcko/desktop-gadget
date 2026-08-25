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
void app_apply_rotation(bool flipped);      /* 180 deg landscape           */
