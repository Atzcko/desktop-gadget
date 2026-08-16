/**
 * config.example.h — template for config.h
 *
 * Copy to config.h and fill in. config.h is gitignored and must never
 * be committed: it holds your Wi-Fi password.
 *
 *     cp include/config.example.h include/config.h
 */
#pragma once

/* ---------------------------------------------------------------- Wi-Fi -- */
#define WIFI_SSID               "your-ssid"
#define WIFI_PASSWORD           "your-password"

/* Wi-Fi is not used until Stage 2. Stage 1 ignores these. */

/* ------------------------------------------------------------- Location -- */
/* Used by Open-Meteo. Decimal degrees; south/west are negative. */
#define WEATHER_LATITUDE        44.7866
#define WEATHER_LONGITUDE       20.4489

/* "celsius" or "fahrenheit" — passed straight to the Open-Meteo query. */
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
 *   Central Europe   "CET-1CEST,M3.5.0,M10.5.0/3"
 *   UK               "GMT0BST,M3.5.0/1,M10.5.0"
 *   US Eastern       "EST5EDT,M3.2.0,M11.1.0"
 *   US Pacific       "PST8PDT,M3.2.0,M11.1.0"
 *   UTC              "UTC0"
 */
#define TIMEZONE_POSIX          "CET-1CEST,M3.5.0,M10.5.0/3"

/* Full NTP resync interval, in hours. */
#define NTP_RESYNC_HOURS        24

/* ------------------------------------------------------------ Brightness -- */
/*
 * Panel brightness is a raw 0..255 register value written straight to the
 * RM690B0. It is NOT linear in perceived luminance — 90 looks brighter
 * than "35%" suggests. Tune by eye.
 * See docs/decisions/D008 - Brightness scale.md
 */
#define BRIGHTNESS_DAY          90      /* ~35% of max */
#define BRIGHTNESS_NIGHT        25      /* ~10% of max */

/* Night runs from NIGHT_START_HOUR until NIGHT_END_HOUR, local time.
 * Wrapping past midnight is expected and handled. 24h clock. */
#define NIGHT_START_HOUR        22
#define NIGHT_END_HOUR          7

/* --------------------------------------------------------------- Network -- */
#define MDNS_HOSTNAME           "flipclock"     /* -> flipclock.local */
#define HTTP_PORT               80

/* ------------------------------------------------------------------ Care -- */
/* Anti burn-in: layout drifts on a random walk bounded to +/- this many px. */
#define BURNIN_SHIFT_PX         2
#define BURNIN_STEP_SECONDS     180
