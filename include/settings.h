/**
 * settings.h — persistent, on-device-editable configuration.
 *
 * config.h supplies FIRST-BOOT DEFAULTS only. Once anything has been saved,
 * NVS wins. Re-flashing firmware does not clear NVS, so settings survive
 * updates; settings_reset() is the only thing that restores defaults.
 *
 * Everything in the firmware reads settings_get(); nothing reads config.h
 * directly except settings_reset().
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

struct Settings {
    /* Network */
    char    wifi_ssid[33];      /* 32 chars max per 802.11 + NUL */
    char    wifi_pass[65];      /* 64 chars max for WPA2 PSK + NUL */

    /* Time */
    char    tz_posix[52];       /* POSIX TZ string, not an IANA name */
    bool    use_24h;

    /* Location */
    char    city[40];
    float   latitude;
    float   longitude;

    /* Display */
    uint8_t brightness_day;     /* raw panel register, 0..255 */
    uint8_t brightness_night;
    uint8_t night_start_hour;
    uint8_t night_end_hour;
    bool    show_weather;
    bool    burnin_guard;
};

/* Load from NVS, falling back to config.h defaults for anything absent. */
void      settings_load(void);

/* Persist the current values. Only writes keys whose value actually
 * changed — NVS is flash, and this device runs for months. */
void      settings_save(void);

/* Restore config.h defaults and persist them. */
void      settings_reset(void);

Settings &settings_get(void);

/* ---------------------------------------------------------- timezones -- */
/* A curated table rather than full tzdata: the IANA database is ~450 KB and
 * the device only needs the handful of zones a human would scroll to. */
struct TimezoneEntry {
    const char *label;   /* what the user sees */
    const char *posix;   /* what libc gets */
};

extern const TimezoneEntry TIMEZONES[];
extern const int           TIMEZONE_COUNT;

/* Index of the currently-selected zone, or 0 if it is not in the table. */
int  settings_timezone_index(void);
