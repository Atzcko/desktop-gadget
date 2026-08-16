/**
 * Desktop gadget — LilyGO T4-S3 Fliqlo flip clock
 *
 *   clock + weather + on-device settings (hold the screen for 5 seconds)
 */
#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <time.h>

#include "config.h"
#include "settings.h"
#include "net.h"
#include "ui.h"
#include "ui_settings.h"
#include "app.h"

LilyGo_Class amoled;

static int      last_min = -1;
static int      last_hour = -1;
static uint8_t  applied_brightness = 0;
static int      off_x = 0, off_y = 0;
static uint32_t next_burnin_ms = 0;

/* -------------------------------------------------------- host callbacks -- */

void app_apply_brightness(uint8_t level)
{
    if (level == applied_brightness) return;
    applied_brightness = level;
    amoled.setBrightness(level);
}

void app_refresh_clock(bool animate)
{
    struct tm tm_now;
    if (!getLocalTime(&tm_now, 5)) return;
    last_hour = tm_now.tm_hour;
    last_min  = tm_now.tm_min;
    ui_set_time(tm_now.tm_hour, tm_now.tm_min, animate);
}

void app_show_info_overlay(void)
{
    struct tm tm_now;
    char date_line[64] = "date unavailable";
    char sync_line[64];

    if (getLocalTime(&tm_now, 5)) {
        strftime(date_line, sizeof(date_line), "%A %d %B %Y", &tm_now);
    }

    WeatherData w  = net_weather();
    NetStatus   st = net_status();
    if (w.valid && st.time_valid) {
        long age = (long)(time(nullptr) - w.last_sync);
        if (age < 90)        snprintf(sync_line, sizeof(sync_line), "weather just now");
        else if (age < 5400) snprintf(sync_line, sizeof(sync_line), "weather %ld min ago", age / 60);
        else                 snprintf(sync_line, sizeof(sync_line), "weather %ld h ago", age / 3600);
    } else {
        snprintf(sync_line, sizeof(sync_line), "weather never synced");
    }
    ui_show_info(date_line, sync_line);
}

/* ------------------------------------------------------------- schedules -- */

static bool is_night(int hour)
{
    Settings &s = settings_get();
    if (s.night_start_hour == s.night_end_hour) return false;
    if (s.night_start_hour < s.night_end_hour) {
        return hour >= s.night_start_hour && hour < s.night_end_hour;
    }
    /* Wraps past midnight, which is the normal case. */
    return hour >= s.night_start_hour || hour < s.night_end_hour;
}

/* Bounded random walk: one pixel at a time, never more than
 * BURNIN_SHIFT_PX from centre, so it is imperceptible in normal use but
 * keeps the lit pixels moving over hours. */
static void burnin_step(void)
{
    Settings &s = settings_get();
    if (!s.burnin_guard) {
        if (off_x || off_y) { off_x = off_y = 0; ui_set_offset(0, 0); }
        return;
    }
    int dx = (int)(esp_random() % 3) - 1;
    int dy = (int)(esp_random() % 3) - 1;
    off_x += dx;
    off_y += dy;
    if (off_x >  BURNIN_SHIFT_PX) off_x =  BURNIN_SHIFT_PX;
    if (off_x < -BURNIN_SHIFT_PX) off_x = -BURNIN_SHIFT_PX;
    if (off_y >  BURNIN_SHIFT_PX) off_y =  BURNIN_SHIFT_PX;
    if (off_y < -BURNIN_SHIFT_PX) off_y = -BURNIN_SHIFT_PX;
    ui_set_offset(off_x, off_y);
}

/* ------------------------------------------------------------------ boot -- */

void setup()
{
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1200) delay(10);

    Serial.println("\n=== Desktop gadget — flip clock ===");

    settings_load();
    Settings &s = settings_get();

    if (!amoled.beginAMOLED_241(/*disable_sd=*/true, /*disable_state_led=*/false)) {
        Serial.println("FATAL: beginAMOLED_241() failed");
        while (true) delay(1000);
    }

    Serial.printf("Panel   : %u x %u\n", amoled.width(), amoled.height());
    Serial.printf("Touch   : %s\n", amoled.hasTouch() ? "online" : "OFFLINE");
    Serial.printf("City    : %s (%.4f, %.4f)\n", s.city, s.latitude, s.longitude);
    Serial.printf("TZ      : %s\n", s.tz_posix);
    Serial.printf("SSID    : %s\n", s.wifi_ssid[0] ? s.wifi_ssid : "(unset)");

    beginLvglHelper(amoled);

    ui_init(amoled.width(), amoled.height());
    ui_show_weather_block(s.show_weather);

    /* Paint something immediately — the panel is lit within a second even
     * though NTP has not landed yet. */
    ui_set_time(0, 0, false);
    app_apply_brightness(s.brightness_day);

    net_begin();

    next_burnin_ms = millis() + (uint32_t)BURNIN_STEP_SECONDS * 1000u;
    Serial.printf("UI up in %lu ms\n", (unsigned long)millis());
}

/* ------------------------------------------------------------------ loop -- */

void loop()
{
    lv_timer_handler();

    static uint32_t next_tick = 0;
    uint32_t now = millis();

    if (now >= next_tick) {
        next_tick = now + 200;

        if (!ui_settings_is_open()) {
            struct tm tm_now;
            if (getLocalTime(&tm_now, 0)) {
                if (tm_now.tm_min != last_min || tm_now.tm_hour != last_hour) {
                    /* First paint after NTP lands must not animate — it
                     * would fold from a meaningless 00:00. */
                    bool animate = (last_min >= 0);
                    last_hour = tm_now.tm_hour;
                    last_min  = tm_now.tm_min;
                    ui_set_time(tm_now.tm_hour, tm_now.tm_min, animate);
                }

                Settings &s = settings_get();
                app_apply_brightness(is_night(tm_now.tm_hour) ? s.brightness_night
                                                              : s.brightness_day);
            }

            WeatherData w = net_weather();
            ui_set_weather(w.current, w.lo, w.hi, w.valid, w.stale);

            if (now >= next_burnin_ms) {
                next_burnin_ms = now + (uint32_t)BURNIN_STEP_SECONDS * 1000u;
                burnin_step();
            }
        }
    }

    delay(2);
}
