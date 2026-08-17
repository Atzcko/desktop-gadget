/**
 * Desktop gadget — LilyGO T4-S3 Fliqlo flip clock
 *
 *   clock + weather + on-device settings (hold the screen for 3 seconds)
 */
#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <time.h>
#include <WiFi.h>

#include "version.h"
#include "config.h"
#include "settings.h"
#include "net.h"
#include "ui.h"
#include "ui_settings.h"
#include "app.h"
#include "emotion.h"
#include "ble.h"
#include "httpapi.h"

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

/*
 * Rotation 2 is the OTHER landscape: same 600x450, and the library re-applies
 * the CST226SE swap/mirror with it, so touch follows the screen. Panel
 * dimensions are unchanged either way, which is why LVGL needs no re-init and
 * this can be toggled live from Settings.
 */
void app_apply_rotation(bool flipped)
{
    amoled.setRotation(flipped ? 2 : 0);

    /* This is called once at boot BEFORE beginLvglHelper(), where lv_scr_act()
     * is still NULL — dereferencing it panics with LoadProhibited at address
     * 0x8. Only the runtime toggle from Settings needs a repaint anyway; the
     * boot call happens before anything has been drawn. */
    if (lv_disp_get_default()) lv_obj_invalidate(lv_scr_act());
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

    uint32_t age = net_weather_age_s();
    if (age == UINT32_MAX)   snprintf(sync_line, sizeof(sync_line), "weather never synced");
    else if (age < 90)       snprintf(sync_line, sizeof(sync_line), "weather just now");
    else if (age < 5400)     snprintf(sync_line, sizeof(sync_line), "weather %u min ago", age / 60);
    else                     snprintf(sync_line, sizeof(sync_line), "weather %u h ago", age / 3600);
    ui_show_info(date_line, sync_line);
}

/* ------------------------------------------------------------- schedules -- */

/*
 * Prefer the SUN over the clock.
 *
 * A fixed 21:00-07:00 window is wrong twice a year and wrong every morning
 * between sunrise and the window's end: at 06:10 with sunrise at 05:58 the sun
 * is up, the panel is bright daylight-lit, and a schedule-driven device is
 * still sitting at night brightness.
 *
 * Open-Meteo already gives us is_day for the weather icon, computed from
 * sunrise/sunset at our exact coordinates — so this is free, it tracks the
 * seasons, and it follows the city if that changes. The hour window remains as
 * the fallback for when weather has never arrived, and can be forced from
 * Settings for anyone who wants fixed hours.
 */
static bool is_night(int hour)
{
    Settings &s = settings_get();

    if (s.night_follows_sun) {
        WeatherData w = net_weather();
        if (w.valid) return !w.is_day;
    }

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

    Serial.printf("\n=== Desktop gadget — flip clock  v%s  %s  (%s) ===\n",
                  FW_VERSION, FW_GIT, FW_BUILD);

    settings_load();
    Settings &s = settings_get();

    /*
     * disable_sd:        the slot is unused, and SD.begin() costs boot time we
     *                    need for the <10 s cold-boot target.
     * disable_state_led: the SY6970 charge LED blinks red because there is NO
     *                    BATTERY — the charger can never complete a cycle, so
     *                    it reports a fault, and a blink is how it says so.
     *                    Nothing is wrong; it is a charger complaining about a
     *                    battery that was never fitted. On an always-on desk
     *                    object whose whole aesthetic is pixels genuinely off,
     *                    a red light flashing forever on the back is the only
     *                    thing in the room that looks broken. See D007.
     */
    if (!amoled.beginAMOLED_241(/*disable_sd=*/true, /*disable_state_led=*/true)) {
        Serial.println("FATAL: beginAMOLED_241() failed");
        while (true) delay(1000);
    }

    /* Before LVGL starts, so the first frame is already the right way up. */
    app_apply_rotation(s.rotate_180);

    Serial.printf("Panel   : %u x %u  %s\n", amoled.width(), amoled.height(),
                  s.rotate_180 ? "(180)" : "");
    Serial.printf("Touch   : %s\n", amoled.hasTouch() ? "online" : "OFFLINE");
    Serial.printf("City    : %s (%.4f, %.4f)\n", s.city, s.latitude, s.longitude);
    Serial.printf("TZ      : %s\n", s.tz_posix);
    settings_dump("restored from NVS");

    beginLvglHelper(amoled);

    ui_init(amoled.width(), amoled.height());
    ui_show_weather_block(s.show_weather);
    ui_show_humidity(s.show_humidity);

    /* Paint something immediately — the panel is lit within a second even
     * though NTP has not landed yet. */
    ui_set_time(0, 0, false);
    app_apply_brightness(s.brightness_day);

    emotion_begin();

    /*
     * ORDER MATTERS. NimBLEDevice::init() -> esp_bt_controller_enable()
     * brings up radio coexistence, and on arduino-esp32 2.x that aborts
     * inside coex_core_enable() if Wi-Fi has already claimed the 2.4 GHz
     * radio. Starting BLE first and Wi-Fi second lets the coexistence
     * scheme be established once, cleanly.
     *
     * Symptom if you swap these back: an immediate, endless boot loop with
     *   abort() ... coex_core_enable <- coex_enable <- esp_bt_controller_enable
     */
    ble_begin();
    net_begin();

    next_burnin_ms = millis() + (uint32_t)BURNIN_STEP_SECONDS * 1000u;
    Serial.printf("UI up in %lu ms\n", (unsigned long)millis());
}

/* ------------------------------------------------------------------ loop -- */

void loop()
{
    lv_timer_handler();
    emotion_tick();

    /* The HTTP server and mDNS both need a live IP, so they start on the
     * first successful association rather than in setup(). */
    static bool http_up = false;
    if (!http_up && WiFi.status() == WL_CONNECTED) {
        http_up = true;
        httpapi_begin();
    }

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
            ui_set_weather(w.current, w.lo, w.hi, w.humidity, w.code, w.is_day, w.valid, w.stale);

            if (now >= next_burnin_ms) {
                next_burnin_ms = now + (uint32_t)BURNIN_STEP_SECONDS * 1000u;
                burnin_step();
            }
        }
    }

    delay(2);
}
