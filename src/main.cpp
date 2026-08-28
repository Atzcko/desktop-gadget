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
#include "gauge.h"
#include "msg.h"
#include "yt.h"
#include "app_api.h"
#include "ui_settings.h"
#include "app.h"
#include "app_host.h"
#include "script.h"
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

static BatteryState g_batt;
BatteryState app_battery(void) { return g_batt; }

/*
 * Voltage -> percent, resting LiPo curve, linear between points. Honest about
 * what it is: under load it reads low, on the charger it reads high (the CV
 * phase holds the cell at 4.2 V long before it is full). D042.
 */
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
void app_apply_rotation(uint8_t rotation)
{
    amoled.setRotation(rotation & 3);

    /* Called once at boot BEFORE beginLvglHelper(), where lv_scr_act() is
     * still NULL — dereferencing it panics with LoadProhibited at 0x8. */
    if (lv_disp_get_default()) lv_obj_invalidate(lv_scr_act());
}

void app_panel_rotate(uint8_t rotation)
{
    amoled.setRotation(rotation & 3);
    lv_disp_t *d = lv_disp_get_default();
    if (!d) return;
    /* The driver struct lives inside the library's LV_Helper; the display
     * keeps a pointer to it, and lv_disp_drv_update() re-reads the fields. */
    d->driver->hor_res = amoled.width();
    d->driver->ver_res = amoled.height();
    lv_disp_drv_update(d, d->driver);
}

void app_apply_rotation_live(uint8_t rotation)
{
    /*
     * Rotate IN PLACE (v1.24.1): remember what was open, tear it down through
     * the host's own paths, rotate, rebuild the clock for the new shape, then
     * reopen what was open — which lays itself out for the new shape too
     * (or re-borrows landscape, if it is a script that never opted in).
     *
     * v1.22.0 just went home, on the argument that orientation is a
     * device-level act. The owner's actual hands disagreed: you rotate the
     * thing while USING the thing, and being thrown to the clock reads as a
     * crash, not a policy. The one real casualty is unsaved Settings edits —
     * create-on-entry apps cannot be rebuilt mid-edit — and rotating with the
     * BOOT button mid-edit is judged rarer than rotating mid-timer.
     */
    const App *keep       = app_host_running();
    const bool drawer_was = app_host_is_open() && !keep;
    if (app_host_is_open()) app_host_home();    /* destroys + restores forced rot */

    app_panel_rotate(rotation);
    ui_init(amoled.width(), amoled.height());   /* re-entrant since D043 */

    Settings &s = settings_get();
    app_refresh_clock(false);
    ui_show_weather_block(s.show_weather);
    ui_show_humidity(s.show_humidity);
    ui_set_battery(g_batt.present, g_batt.pct, g_batt.charging);

    if      (keep)       app_host_launch(keep);      /* fresh, in the new shape */
    else if (drawer_was) app_host_open_drawer();
    Serial.printf("[rot] now %dx90, %ux%u, back in %s\n",
                  rotation, amoled.width(), amoled.height(),
                  keep ? keep->name : (drawer_was ? "drawer" : "clock"));
}

static volatile int  pending_rot = -1;
static volatile bool pending_rebuild;
void app_request_rotation(uint8_t rotation) { pending_rot = rotation & 3; }
void app_request_rebuild(void)              { pending_rebuild = true; }

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
    app_apply_rotation(s.rotation);

    Serial.printf("Panel   : %u x %u  %s\n", amoled.width(), amoled.height(),
                  s.rotation ? "(rotated)" : "");
    Serial.printf("Touch   : %s\n", amoled.hasTouch() ? "online" : "OFFLINE");
    Serial.printf("City    : %s (%.4f, %.4f)\n", s.city, s.latitude, s.longitude);
    Serial.printf("TZ      : %s\n", s.tz_posix);
    settings_dump("restored from NVS");

    beginLvglHelper(amoled);

    /* BOOT as an orientation button, with the INTERNAL pull-up. Plain INPUT
     * left the pin floating and the very first remote test caught a phantom
     * press corrupting an orientation change — the "external 10 K pull-up"
     * this code first trusted was an unverified claim in our own reference
     * note, now corrected. The library's own examples use INPUT_PULLUP. */
    pinMode(0, INPUT_PULLUP);

    gauge_begin(amoled.getBattVoltage());
    msg_begin();
    yt_begin();

    ui_init(amoled.width(), amoled.height());
    ui_show_weather_block(s.show_weather);
    ui_show_humidity(s.show_humidity);

    /* Paint something immediately — the panel is lit within a second even
     * though NTP has not landed yet. */
    ui_set_time(0, 0, false);
    app_apply_brightness(s.brightness_day);

    emotion_begin();

    /* Scripts are discovered before anything can open the drawer. LittleFS
     * formats itself on first boot, so this is also where a fresh device
     * grows its /apps directory. */
    script_begin();

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
    app_host_tick();

    /* The HTTP server and mDNS both need a live IP, so they start on the
     * first successful association rather than in setup(). */
    static bool http_up = false;
    if (!http_up && WiFi.status() == WL_CONNECTED) {
        http_up = true;
        httpapi_begin();
    }

    static uint32_t next_tick = 0;
    uint32_t now = millis();

    /*
     * Battery, every 30 s, from THIS task: the PMU shares the internal I2C
     * bus with the touch controller, and single-master discipline is the
     * same rule as D018, applied to a bus instead of a widget tree. First
     * read at 3 s so /health is honest soon after boot.
     */
    /*
     * BOOT (GPIO0) cycles the orientation, 90 degrees per press. The pin is
     * only a strapping pin at reset; in normal run it is a plain input with
     * an external 10 K pull-up, LOW while pressed (see T4-S3 note). Judged on
     * RELEASE with a 30 ms debounce; presses longer than 2 s are ignored so
     * a hand resting on the button while repositioning the device does not
     * spin the screen.
     */
    static uint32_t bt0_down_ms = 0;
    static bool     bt0_was_low = false;
    const bool bt0_low = (digitalRead(0) == LOW);
    if (bt0_low && !bt0_was_low) {
        bt0_down_ms = now;
    } else if (!bt0_low && bt0_was_low) {
        const uint32_t held = now - bt0_down_ms;
        if (held >= 30 && held <= 2000) {
            Settings &sr = settings_get();
            sr.rotation = (uint8_t)((sr.rotation + 1) & 3);
            settings_save();
            app_apply_rotation_live(sr.rotation);
        }
    }
    bt0_was_low = bt0_low;

    if (pending_rebuild) {
        pending_rebuild = false;
        app_apply_rotation_live(settings_get().rotation);
    }

    /* Rotation asked for over HTTP: same path, served on this task. */
    if (pending_rot >= 0) {
        const uint8_t r = (uint8_t)pending_rot;
        pending_rot = -1;
        Settings &sr = settings_get();
        if (r != sr.rotation) {
            sr.rotation = r;
            settings_save();
            app_apply_rotation_live(r);
        }
    }

    static uint32_t next_batt = 3000;
    if (now >= next_batt) {
        next_batt = now + 30000;

        /*
         * DO NOT use amoled.isBatteryConnect() here. For this board it is
         * implemented as `getVbusVoltage() != 0` — it answers "is USB
         * plugged in", because the real SY6970 battery-detect is marked
         * error("Not implemented") in XPowersLib and LilyGO papered over it.
         * Trusting it made the chip vanish the moment USB was unplugged:
         * hidden exactly when running on battery.
         *
         * Presence is judged from the cell voltage instead. A real 1S cell
         * lives in 2800-4400 mV; with no battery the BAT pin reads either
         * ~0 (floating ADC) or VSYS regulation (~4.5 V+) — both outside the
         * window. Two consecutive out-of-window reads are required to flip
         * to absent, so one bad read on the shared I2C bus cannot blink the
         * indicator off.
         */
        const uint16_t mv = amoled.getBattVoltage();
        const bool in_window = (mv >= 2800 && mv <= 4400);
        static uint8_t absent_reads = 0;
        if (in_window) absent_reads = 0;
        else if (absent_reads < 2) absent_reads++;

        g_batt.mv       = mv;
        g_batt.present  = in_window || absent_reads < 2;
        g_batt.vbus     = amoled.isVbusIn();

        /* chargeStatus() read directly: the library's isChargeDone() helper
         * returns the OPPOSITE of its name (!= where == belongs) and its
         * isCharging() counts DONE as charging. Same genre as
         * isBatteryConnect() answering a different question (D042). */
        const auto cs = amoled.SY.chargeStatus();
        const bool chg_done = (cs == PowersSY6970::CHARGE_STATE_DONE);
        g_batt.charging = g_batt.present && g_batt.vbus &&
                          (cs == PowersSY6970::CHARGE_STATE_PRE_CHARGE ||
                           cs == PowersSY6970::CHARGE_STATE_FAST_CHARGE);

        static uint32_t last_gauge_ms = 0;
        const uint32_t g_dt = last_gauge_ms ? (now - last_gauge_ms) : 0;
        last_gauge_ms = now;
        if (g_dt > 0)
            gauge_update(mv, g_batt.present, g_batt.charging, chg_done,
                         g_batt.charging ? amoled.SY.getChargeCurrent() : 0,
                         applied_brightness, g_dt);

        g_batt.pct = g_batt.present ? gauge_pct() : -1;
        ui_set_battery(g_batt.present, g_batt.pct, g_batt.charging);
    }

    /* The clock's unread badge follows the message store (D047). Cheap:
     * two volatile reads and a comparison. */
    static int shown_unread = -1;
    if (msg_unread() != shown_unread) {
        shown_unread = msg_unread();
        ui_set_unread(shown_unread);
    }

    if (now >= next_tick) {
        next_tick = now + 200;

        if (!ui_settings_is_open() && !app_host_is_open()) {
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
