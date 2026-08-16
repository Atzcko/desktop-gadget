#include "net.h"
#include "settings.h"
#include "config.h"
#include "ble.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static WeatherData      wx;
static SemaphoreHandle_t wx_lock;
static volatile bool     wx_fetch_now = false;
static volatile uint8_t  last_disconnect;
static volatile uint32_t reconnect_at_ms;
static uint32_t          reconnect_backoff_ms = 2000;
static time_t            last_ntp = 0;

/* ------------------------------------------------------------- helpers -- */

static bool https_get(const String &url, String &body)
{
    WiFiClientSecure client;
    /* Open-Meteo serves public forecast data and carries no credentials of
     * ours. Pinning a CA would mean shipping and rotating a root bundle on
     * a device with no update path, for data that is not sensitive. */
    client.setInsecure();
    client.setTimeout(8000);

    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[net] GET %d for %s\n", code, url.c_str());
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    return true;
}

/* ------------------------------------------------------------- weather -- */

static bool fetch_weather(void)
{
    Settings &s = settings_get();

    String url = "https://api.open-meteo.com/v1/forecast";
    url += "?latitude=";  url += String(s.latitude, 4);
    url += "&longitude="; url += String(s.longitude, 4);
    url += "&current=temperature_2m,relative_humidity_2m";
    url += "&daily=temperature_2m_max,temperature_2m_min";
    url += "&forecast_days=1&timezone=auto&temperature_unit=";
    url += WEATHER_TEMPERATURE_UNIT;

    String body;
    if (!https_get(url, body)) return false;

    StaticJsonDocument<1536> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[net] weather JSON: %s\n", err.c_str());
        return false;
    }

    if (!doc["current"]["temperature_2m"].is<float>()) return false;

    float cur = doc["current"]["temperature_2m"].as<float>();
    float hi  = doc["daily"]["temperature_2m_max"][0] | cur;
    float lo  = doc["daily"]["temperature_2m_min"][0] | cur;
    /* -1 marks "not reported" so the UI can hide the field rather than
     * confidently displaying 0 %. */
    float rh  = doc["current"]["relative_humidity_2m"] | -1.0f;

    xSemaphoreTake(wx_lock, portMAX_DELAY);
    wx.current   = cur;
    wx.lo        = lo;
    wx.hi        = hi;
    wx.humidity  = rh;
    wx.valid     = true;
    wx.stale     = false;
    wx.last_sync = time(nullptr);
    xSemaphoreGive(wx_lock);

    Serial.printf("[net] weather %.1f (%.1f..%.1f) rh %.0f%%\n", cur, lo, hi, rh);
    return true;
}

static void mark_stale(void)
{
    xSemaphoreTake(wx_lock, portMAX_DELAY);
    wx.stale = true;
    xSemaphoreGive(wx_lock);
}

static void weather_task(void *)
{
    const uint32_t normal_ms  = (uint32_t)WEATHER_REFRESH_MINUTES * 60u * 1000u;
    const uint32_t backoff_max = normal_ms;   /* never back off past the
                                                 normal interval           */
    uint32_t backoff_ms = 30000;
    uint32_t wait_ms    = 3000;               /* first attempt, once Wi-Fi
                                                 has had a moment          */

    for (;;) {
        /* Sleep in 250 ms slices so a "fetch now" request from the Settings
         * screen is honoured promptly instead of after 15 minutes. */
        uint32_t waited = 0;
        while (waited < wait_ms && !wx_fetch_now) {
            vTaskDelay(pdMS_TO_TICKS(250));
            waited += 250;
        }
        wx_fetch_now = false;

        if (WiFi.status() != WL_CONNECTED) {
            /* The retry lives here rather than in the event handler so it is
             * rate-limited and off the event task. */
            uint32_t due = reconnect_at_ms;
            if (due && (int32_t)(millis() - due) >= 0) {
                reconnect_at_ms = 0;
                Settings &st = settings_get();
                if (st.wifi_ssid[0]) {
                    Serial.println("[net] retrying association");
                    WiFi.disconnect();
                    WiFi.begin(st.wifi_ssid, st.wifi_pass);
                }
            }
            mark_stale();
            wait_ms = 3000;
            continue;
        }

        if (fetch_weather()) {
            backoff_ms = 30000;
            wait_ms    = normal_ms;
        } else {
            mark_stale();
            wait_ms    = backoff_ms;
            backoff_ms = (backoff_ms * 2 > backoff_max) ? backoff_max : backoff_ms * 2;
            Serial.printf("[net] weather failed, retry in %lu s\n",
                          (unsigned long)(wait_ms / 1000));
        }
    }
}

/* ---------------------------------------------------------------- wifi -- */

/* Reason codes worth recognising on sight, from esp_wifi_types.h. */
const char *net_disconnect_text(uint8_t r)
{
    switch (r) {
    case 2:   return "AUTH_EXPIRE";
    case 4:   return "ASSOC_EXPIRE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT (wrong password?)";
    case 201: return "NO_AP_FOUND (wrong SSID, or 5 GHz-only network?)";
    case 202: return "AUTH_FAIL (wrong password)";
    case 203: return "ASSOC_FAIL";
    case 205: return "CONNECTION_FAIL";
    default:  return "see esp_wifi_types.h";
    }
}

static void on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        reconnect_backoff_ms = 2000;
        reconnect_at_ms      = 0;
        last_disconnect      = 0;
        Serial.printf("[net] IP %s  RSSI %d\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        /* Kick NTP as soon as we have a route. */
        configTzTime(settings_get().tz_posix, NTP_SERVER,
                     "time.nist.gov", "time.cloudflare.com");
        wx_fetch_now = true;
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
        uint8_t reason  = info.wifi_sta_disconnected.reason;
        last_disconnect = reason;
        /* Reconnecting immediately, from inside the event handler, produces
         * a tight loop that floods the log and never lets the supplicant
         * settle. Schedule it instead, with backoff, and let the weather
         * task perform the retry. */
        reconnect_at_ms = millis() + reconnect_backoff_ms;
        Serial.printf("[net] disconnected: reason %u %s — retry in %lu s\n",
                      reason, net_disconnect_text(reason),
                      (unsigned long)(reconnect_backoff_ms / 1000));
        reconnect_backoff_ms = (reconnect_backoff_ms >= 30000)
                             ? 30000 : reconnect_backoff_ms * 2;
        break;
    }
    default:
        break;
    }
}

void net_begin(void)
{
    wx_lock = xSemaphoreCreateMutex();
    memset(&wx, 0, sizeof(wx));

    Settings &s = settings_get();

    /* Set the zone before anything reads the clock, so even the pre-NTP
     * garbage time is at least in the right frame. */
    setenv("TZ", s.tz_posix, 1);
    tzset();

    WiFi.onEvent(on_wifi_event);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    /*
     * Wi-Fi modem sleep is NOT optional when Bluetooth is also enabled. The
     * two share one 2.4 GHz radio, and modem sleep is the mechanism by which
     * Wi-Fi yields airtime to BT. Disabling it aborts the Wi-Fi task with:
     *
     *   E wifi: Error! Should enable WiFi modem sleep when both WiFi and
     *           Bluetooth are enabled!!!!!!
     *
     * With BLE off we can still keep it disabled for slightly snappier HTTP.
     */
    WiFi.setSleep(ble_is_running());

    if (s.wifi_ssid[0]) {
        Serial.printf("[net] connecting to \"%s\"\n", s.wifi_ssid);
        WiFi.begin(s.wifi_ssid, s.wifi_pass);
    } else {
        Serial.println("[net] no SSID configured — hold the screen 3 s to set one");
    }

    xTaskCreatePinnedToCore(weather_task, "weather", 6144, nullptr, 1, nullptr, 0);
}

void net_apply_wifi(const char *ssid, const char *pass)
{
    WiFi.disconnect(false, true);
    if (ssid && ssid[0]) {
        Serial.printf("[net] reconnecting to \"%s\"\n", ssid);
        WiFi.begin(ssid, pass);
    }
}

void net_apply_timezone(const char *posix_tz)
{
    setenv("TZ", posix_tz, 1);
    tzset();
    if (WiFi.status() == WL_CONNECTED) {
        configTzTime(posix_tz, NTP_SERVER, "time.nist.gov", "time.cloudflare.com");
    }
}

void net_request_weather_now(void) { wx_fetch_now = true; }

/* --------------------------------------------------------------- state -- */

NetStatus net_status(void)
{
    NetStatus st;
    memset(&st, 0, sizeof(st));
    st.wifi_up = (WiFi.status() == WL_CONNECTED);
    st.rssi    = st.wifi_up ? WiFi.RSSI() : 0;
    if (st.wifi_up) {
        strncpy(st.ip, WiFi.localIP().toString().c_str(), sizeof(st.ip) - 1);
    }
    time_t now = time(nullptr);
    /* Anything before 2021 means NTP has not landed yet. */
    st.time_valid = (now > 1609459200);
    if (st.time_valid && !last_ntp) last_ntp = now;
    st.last_ntp = last_ntp;
    return st;
}

uint8_t net_last_disconnect(void) { return last_disconnect; }

WeatherData net_weather(void)
{
    WeatherData copy;
    xSemaphoreTake(wx_lock, portMAX_DELAY);
    copy = wx;
    xSemaphoreGive(wx_lock);
    return copy;
}

/* ---------------------------------------------------------------- scan -- */

int net_scan(char ssids[][33], int *rssi, bool *secured, int max)
{
    int n = WiFi.scanNetworks();
    if (n < 0) return 0;

    int out = 0;
    for (int i = 0; i < n && out < max; i++) {
        String ss = WiFi.SSID(i);
        if (ss.length() == 0) continue;          /* hidden network */

        bool dup = false;                        /* mesh APs repeat SSIDs */
        for (int j = 0; j < out; j++) {
            if (ss.equals(ssids[j])) { dup = true; break; }
        }
        if (dup) continue;

        strncpy(ssids[out], ss.c_str(), 32);
        ssids[out][32] = '\0';
        rssi[out]      = WiFi.RSSI(i);
        secured[out]   = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        out++;
    }
    WiFi.scanDelete();
    return out;
}

/* ----------------------------------------------------------- geocoding -- */

int net_geocode(const char *query, GeoResult *out, int max)
{
    if (WiFi.status() != WL_CONNECTED) return 0;

    String q;
    for (const char *p = query; *p; p++) {       /* minimal URL encoding */
        if (isalnum((unsigned char)*p)) q += *p;
        else if (*p == ' ')             q += "%20";
        else                            q += '%', q += String((uint8_t)*p, HEX);
    }

    String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
    url += q;
    url += "&count=" + String(max) + "&language=en&format=json";

    String body;
    if (!https_get(url, body)) return 0;

    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, body)) return 0;

    JsonArray results = doc["results"].as<JsonArray>();
    int n = 0;
    for (JsonObject r : results) {
        if (n >= max) break;
        strncpy(out[n].name,    r["name"]    | "", sizeof(out[n].name) - 1);
        strncpy(out[n].country, r["country"] | "", sizeof(out[n].country) - 1);
        out[n].name[sizeof(out[n].name) - 1]       = '\0';
        out[n].country[sizeof(out[n].country) - 1] = '\0';
        out[n].latitude  = r["latitude"]  | 0.0f;
        out[n].longitude = r["longitude"] | 0.0f;
        n++;
    }
    return n;
}
