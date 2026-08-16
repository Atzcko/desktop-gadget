/**
 * config.example.h — FIRST-BOOT DEFAULTS
 *
 *     cp include/config.example.h include/config.h
 *
 * These are only the factory defaults. Once the device has booted, every
 * value here is editable on-device (hold a finger on the screen for 5
 * seconds) and the edited value is stored in NVS, which wins over this
 * file. Re-flashing does not reset your settings; "Reset to defaults" in
 * the Settings screen does.
 *
 * config.h is gitignored.
 */
#pragma once

/* ---------------------------------------------------------------- Wi-Fi -- */
#define DEFAULT_WIFI_SSID       ""
#define DEFAULT_WIFI_PASSWORD   ""

/*
 * Leave these EMPTY. Wi-Fi is provisioned on-device: hold a finger on the
 * screen for 5 seconds -> Settings -> Wi-Fi, pick your network and type the
 * password there. It is stored in NVS, survives reboots, and never has to
 * live in a file or be shared with anyone.
 *
 * Filling them in here still works as a first-boot fallback if you would
 * rather not type on a 600x450 panel.
 */

/* ------------------------------------------------------------- Location -- */
/* Used by Open-Meteo. Decimal degrees; south/west are negative.
 * Default: Abu Dhabi, UAE. Changeable on-device via Settings. */
#define DEFAULT_LATITUDE        24.4539
#define DEFAULT_LONGITUDE       54.3773

/* "celsius" or "fahrenheit" — passed straight to the Open-Meteo query. */
#define DEFAULT_CITY_NAME       "Abu Dhabi"
#define WEATHER_TEMPERATURE_UNIT "celsius"

/* Degree suffix drawn on screen. */
#define WEATHER_UNIT_SUFFIX     "°"

/* How often to refresh, and how long before the reading is called stale. */
#define WEATHER_REFRESH_MINUTES 15
#define WEATHER_STALE_MINUTES   45

/* ------------------------------------------------------------------ Time -- */
#define NTP_SERVER              "pool.ntp.org"

/*
 * POSIX timezone string — NOT an IANA name like "Europe/Belgrade".
 * libc uses this to handle DST for you, so daylight saving is never
 * something this firmware computes.
 *
 *   Abu Dhabi / Dubai  "<+04>-4"                        (UTC+4, no DST)
 *   Central Europe     "CET-1CEST,M3.5.0,M10.5.0/3"
 *   UK                 "GMT0BST,M3.5.0/1,M10.5.0"
 *   US Eastern         "EST5EDT,M3.2.0,M11.1.0"
 *   UTC                "UTC0"
 *
 * The angle-bracket form is what tzdata itself emits for zones whose
 * abbreviation is a bare numeric offset. Verified against Asia/Dubai.
 */
#define DEFAULT_TIMEZONE_POSIX  "<+04>-4"   /* Abu Dhabi, UTC+4, no DST */

/* Full NTP resync interval, in hours. */
#define NTP_RESYNC_HOURS        24

/* ------------------------------------------------------------ Brightness -- */
/*
 * Panel brightness is a raw 0..255 register value written straight to the
 * RM690B0. It is NOT linear in perceived luminance — 90 looks brighter
 * than "35%" suggests. Tune by eye.
 * See docs/decisions/D008 - Brightness scale.md
 */
#define DEFAULT_BRIGHTNESS_DAY  90      /* ~35% of max */
#define DEFAULT_BRIGHTNESS_NIGHT 25      /* ~10% of max */

/* Night runs from NIGHT_START_HOUR until NIGHT_END_HOUR, local time.
 * Wrapping past midnight is expected and handled. 24h clock. */
#define DEFAULT_NIGHT_START     22
#define DEFAULT_NIGHT_END        7

/* --------------------------------------------------------------- Network -- */
#define MDNS_HOSTNAME           "flipclock"     /* -> flipclock.local */
#define HTTP_PORT               80

/* ------------------------------------------------------------------ Care -- */
/* Anti burn-in: layout drifts on a random walk bounded to +/- this many px. */
#define BURNIN_SHIFT_PX         2
#define BURNIN_STEP_SECONDS     180
