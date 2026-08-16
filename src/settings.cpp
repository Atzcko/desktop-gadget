#include "settings.h"
#include "config.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

static Preferences prefs;
static Settings    s;

/*
 * POSIX TZ strings, not IANA names. The trailing rules encode the DST
 * changeover so libc does daylight saving for us — this firmware never
 * computes a DST transition.
 *
 * The <+04> angle-bracket form is what tzdata emits for zones whose
 * abbreviation is a bare numeric offset; verified against Asia/Dubai,
 * which has no DST in either direction.
 */
const TimezoneEntry TIMEZONES[] = {
    { "Abu Dhabi / Dubai",   "<+04>-4" },
    { "UTC",                 "UTC0" },
    { "London",              "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Central Europe",      "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Athens / Helsinki",   "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Moscow",              "MSK-3" },
    { "Tehran",              "<+0330>-3:30" },
    { "Karachi",             "PKT-5" },
    { "India",               "IST-5:30" },
    { "Dhaka",               "<+06>-6" },
    { "Bangkok / Jakarta",   "<+07>-7" },
    { "Singapore / HK",      "<+08>-8" },
    { "Tokyo / Seoul",       "JST-9" },
    { "Sydney",              "AEST-10AEDT,M10.1.0,M4.1.0/3" },
    { "Auckland",            "NZST-12NZDT,M9.5.0,M4.1.0/3" },
    { "Sao Paulo",           "<-03>3" },
    { "US Eastern",          "EST5EDT,M3.2.0,M11.1.0" },
    { "US Central",          "CST6CDT,M3.2.0,M11.1.0" },
    { "US Mountain",         "MST7MDT,M3.2.0,M11.1.0" },
    { "US Pacific",          "PST8PDT,M3.2.0,M11.1.0" },
    { "Johannesburg",        "SAST-2" },
};
const int TIMEZONE_COUNT = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

/*
 * Arduino's Preferences logs an ESP_LOGE for every absent key even when a
 * default is supplied. The first-boot sentinel handled a virgin device, but
 * not a key ADDED BY A LATER FIRMWARE VERSION — the `blen` (BLE name) key
 * did exactly that and put an error line back in a healthy boot log.
 *
 * These wrappers read a key only when it exists, leave the config.h default
 * in place when it does not, and record that a backfill is needed. That
 * makes adding a setting in future a non-event.
 */
static bool nvs_incomplete;

static bool have(const char *k)
{
    if (prefs.isKey(k)) return true;
    nvs_incomplete = true;
    return false;
}

static void copy_str(char *dst, size_t cap, const char *src)
{
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static void apply_defaults(void)
{
    copy_str(s.wifi_ssid, sizeof(s.wifi_ssid), DEFAULT_WIFI_SSID);
    copy_str(s.wifi_pass, sizeof(s.wifi_pass), DEFAULT_WIFI_PASSWORD);
    copy_str(s.tz_posix,  sizeof(s.tz_posix),  DEFAULT_TIMEZONE_POSIX);
    copy_str(s.city,      sizeof(s.city),      DEFAULT_CITY_NAME);
    s.use_24h          = true;
    s.latitude         = DEFAULT_LATITUDE;
    s.longitude        = DEFAULT_LONGITUDE;
    s.brightness_day   = DEFAULT_BRIGHTNESS_DAY;
    s.brightness_night = DEFAULT_BRIGHTNESS_NIGHT;
    s.night_start_hour = DEFAULT_NIGHT_START;
    s.night_end_hour   = DEFAULT_NIGHT_END;
    s.show_weather     = true;
    s.show_humidity    = true;
    s.burnin_guard     = true;
    s.ble_enabled      = true;
    s.ble_hid          = true;
    s.ble_as_keyboard  = false;
    copy_str(s.ble_name, sizeof(s.ble_name), DEFAULT_BLE_NAME);
}

void settings_load(void)
{
    apply_defaults();

    /*
     * Opened read-WRITE: on a virgin device the namespace does not exist,
     * and Arduino's Preferences logs an ESP_LOGE for the missing namespace
     * AND for every absent key — six error lines on a perfectly healthy
     * first boot, which is worse than useless when you are reading a log
     * to find a real fault.
     *
     * So: probe one sentinel key. If it is missing this is a first boot,
     * and we seed NVS from the config.h defaults instead of reading. Every
     * subsequent boot finds a fully-populated namespace and stays silent.
     */
    prefs.begin("flipclock", /*readOnly=*/false);

    if (!prefs.isKey("tz")) {
        prefs.end();
        Serial.println("[settings] first boot — seeding NVS from config.h defaults");
        settings_save();
        return;
    }

    nvs_incomplete = false;

    if (have("ssid")) copy_str(s.wifi_ssid, sizeof(s.wifi_ssid), prefs.getString("ssid").c_str());
    if (have("pass")) copy_str(s.wifi_pass, sizeof(s.wifi_pass), prefs.getString("pass").c_str());
    if (have("tz"))   copy_str(s.tz_posix,  sizeof(s.tz_posix),  prefs.getString("tz").c_str());
    if (have("city")) copy_str(s.city,      sizeof(s.city),      prefs.getString("city").c_str());
    if (have("blen")) copy_str(s.ble_name,  sizeof(s.ble_name),  prefs.getString("blen").c_str());

    if (have("lat"))    s.latitude         = prefs.getFloat("lat");
    if (have("lon"))    s.longitude        = prefs.getFloat("lon");
    if (have("bday"))   s.brightness_day   = prefs.getUChar("bday");
    if (have("bnight")) s.brightness_night = prefs.getUChar("bnight");
    if (have("nstart")) s.night_start_hour = prefs.getUChar("nstart");
    if (have("nend"))   s.night_end_hour   = prefs.getUChar("nend");
    if (have("h24"))    s.use_24h          = prefs.getBool("h24");
    if (have("wx"))     s.show_weather     = prefs.getBool("wx");
    if (have("hum"))    s.show_humidity    = prefs.getBool("hum");
    if (have("burn"))   s.burnin_guard     = prefs.getBool("burn");
    if (have("ble"))    s.ble_enabled      = prefs.getBool("ble");
    if (have("bhid"))   s.ble_hid          = prefs.getBool("bhid");
    if (have("bkbd"))   s.ble_as_keyboard  = prefs.getBool("bkbd");

    prefs.end();

    /* A key added by a newer firmware build is missing on a device that has
     * already been provisioned. Backfill once so the next boot is silent. */
    if (nvs_incomplete) {
        Serial.println("[settings] backfilling keys added by a newer firmware");
        settings_save();
    }
}

void settings_save(void)
{
    prefs.begin("flipclock", /*readOnly=*/false);

    /* Preferences already skips the write when the stored value is
     * identical, which is what keeps flash wear at zero for a device that
     * sits on a desk for months. */
    prefs.putString("ssid",   s.wifi_ssid);
    prefs.putString("pass",   s.wifi_pass);
    prefs.putString("tz",     s.tz_posix);
    prefs.putString("city",   s.city);
    prefs.putFloat("lat",     s.latitude);
    prefs.putFloat("lon",     s.longitude);
    prefs.putUChar("bday",    s.brightness_day);
    prefs.putUChar("bnight",  s.brightness_night);
    prefs.putUChar("nstart",  s.night_start_hour);
    prefs.putUChar("nend",    s.night_end_hour);
    prefs.putBool("h24",      s.use_24h);
    prefs.putBool("wx",       s.show_weather);
    prefs.putBool("hum",      s.show_humidity);
    prefs.putBool("burn",     s.burnin_guard);
    prefs.putBool("ble",      s.ble_enabled);
    prefs.putBool("bhid",     s.ble_hid);
    prefs.putBool("bkbd",     s.ble_as_keyboard);
    prefs.putString("blen",   s.ble_name);

    prefs.end();
}

void settings_reset(void)
{
    prefs.begin("flipclock", false);
    prefs.clear();
    prefs.end();
    apply_defaults();
    settings_save();
}

Settings &settings_get(void) { return s; }

void settings_dump(const char *tag)
{
    Serial.printf("[settings] %s\n", tag);
    Serial.printf("           ssid=\"%s\" pass=%s\n",
                  s.wifi_ssid, s.wifi_pass[0] ? "(set)" : "(empty)");
    Serial.printf("           city=\"%s\" %.4f,%.4f  tz=\"%s\" 24h=%d\n",
                  s.city, s.latitude, s.longitude, s.tz_posix, s.use_24h);
    Serial.printf("           bright day=%u night=%u  night %02u:00-%02u:00\n",
                  s.brightness_day, s.brightness_night,
                  s.night_start_hour, s.night_end_hour);
    Serial.printf("           weather=%d humidity=%d burnin=%d\n",
                  s.show_weather, s.show_humidity, s.burnin_guard);
    Serial.printf("           ble=%d hid=%d as_keyboard=%d name=\"%s\"\n",
                  s.ble_enabled, s.ble_hid, s.ble_as_keyboard, s.ble_name);
}

int settings_timezone_index(void)
{
    for (int i = 0; i < TIMEZONE_COUNT; i++) {
        if (strcmp(TIMEZONES[i].posix, s.tz_posix) == 0) return i;
    }
    return 0;
}
