/**
 * net.h — Wi-Fi, NTP and Open-Meteo.
 *
 * THREADING CONTRACT: everything here runs on its own FreeRTOS task or on
 * the Arduino event task. Nothing in this file touches an LVGL object. The
 * LVGL loop polls net_weather() / net_status() and does the drawing.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

struct WeatherData {
    float  current;
    float  lo;
    float  hi;
    float  humidity;     /* relative humidity, % */
    int    code;         /* WMO 4677 weather code, -1 if unreported */
    bool   is_day;       /* Open-Meteo derives this from sunrise/sunset */
    bool   valid;        /* have we ever had a good reading?  */
    time_t last_sync;    /* epoch of the last SUCCESSFUL fetch */
    bool   stale;        /* last attempt failed               */
};

struct NetStatus {
    bool     wifi_up;
    int      rssi;
    char     ip[16];
    bool     time_valid;
    time_t   last_ntp;
};

struct GeoResult {
    char  name[40];
    char  country[40];
    float latitude;
    float longitude;
};

/* Starts Wi-Fi, NTP and the weather task. Non-blocking: returns
 * immediately, the clock renders while association is still in progress. */
void net_begin(void);

NetStatus   net_status(void);

/* Last 802.11 disconnect reason code, and a human reading of it. 0 = none.
 * Surfaced in Settings so a wrong password says so instead of just failing. */
uint8_t     net_last_disconnect(void);
const char *net_disconnect_text(uint8_t reason);
WeatherData net_weather(void);

/* Re-associate with new credentials (from the Settings screen) and persist
 * nothing — the caller owns settings_save(). */
void net_apply_wifi(const char *ssid, const char *pass);

/* Re-apply the POSIX TZ string and force an NTP resync. */
void net_apply_timezone(const char *posix_tz);

/* Ask the weather task to fetch now rather than waiting for its interval. */
void net_request_weather_now(void);

/* Blocking Wi-Fi scan. Only safe to call from the LVGL task while the
 * Settings screen is open — it takes a couple of seconds. */
int  net_scan(char ssids[][33], int *rssi, bool *secured, int max);

/* Open-Meteo geocoding: city name -> coordinates. Blocking. */
int  net_geocode(const char *query, GeoResult *out, int max);
