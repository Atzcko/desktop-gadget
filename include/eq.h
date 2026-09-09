/**
 * eq.h — the Equalizer's spectrum worker (D058).
 *
 * The clock has no microphone. The Mac companion hears its own output,
 * runs the FFT, and streams 32 band levels per frame; this worker reads
 * that stream and keeps the latest frame for the app to draw. Never touches
 * LVGL. The companion IP is the YouTube one — same Mac, same process.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define EQ_BANDS 32

bool        eq_start(void);
void        eq_stop(void);
bool        eq_streaming(void);          /* levels are arriving */
const char *eq_status(void);             /* what to show when they are not */
uint32_t    eq_rev(void);                /* bumps per frame or status change */
void        eq_levels(uint8_t out[EQ_BANDS]);
