#include "httpapi.h"
#include "emotion.h"
#include "settings.h"
#include "net.h"
#include "config.h"
#include "ble.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

static AsyncWebServer server(HTTP_PORT);
static bool started;

static void send_err(AsyncWebServerRequest *req, int code, const char *msg)
{
    char body[192];
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", msg);
    req->send(code, "application/json", body);
}

static void handle_health(AsyncWebServerRequest *req)
{
    NetStatus   st = net_status();
    WeatherData w  = net_weather();
    Settings   &s  = settings_get();

    long age = (w.valid && st.time_valid) ? (long)(time(nullptr) - w.last_sync) : -1;

    char body[512];
    snprintf(body, sizeof(body),
             "{\"ok\":true,"
             "\"uptime_s\":%lu,"
             "\"rssi\":%d,"
             "\"ip\":\"%s\","
             "\"ssid\":\"%s\","
             "\"time_synced\":%s,"
             "\"weather_age_s\":%ld,"
             "\"weather_valid\":%s,"
             "\"city\":\"%s\","
             "\"ble\":{\"running\":%s,\"connected\":%s,\"name\":\"%s\"},"
             "\"free_psram\":%u,"
             "\"free_heap\":%u}",
             (unsigned long)(millis() / 1000UL),
             st.rssi,
             st.wifi_up ? st.ip : "",
             s.wifi_ssid,
             st.time_valid ? "true" : "false",
             age,
             w.valid ? "true" : "false",
             s.city,
             ble_is_running()   ? "true" : "false",
             ble_is_connected() ? "true" : "false",
             s.ble_name,
             (unsigned)ESP.getFreePsram(),
             (unsigned)ESP.getFreeHeap());

    req->send(200, "application/json", body);
}

/* Body handler rather than a plain onRequest: ESPAsyncWebServer delivers
 * POST bodies in chunks, and a raw JSON body is not form-encoded so it does
 * not arrive as a parameter. */
static void handle_emotion_body(AsyncWebServerRequest *req, uint8_t *data,
                                size_t len, size_t index, size_t total)
{
    static String buf;
    if (index == 0) buf = "";
    buf.concat((const char *)data, len);
    if (index + len != total) return;      /* wait for the rest */

    EmotionRequest r;
    char err[128];
    if (!emotion_parse(buf.c_str(), &r, err, sizeof(err))) {
        send_err(req, 400, err);
        return;
    }
    if (!emotion_post(r)) {
        send_err(req, 503, "queue full");
        return;
    }
    char body[160];
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"state\":\"%s\",\"duration_s\":%u}",
             emotion_name(r.state), r.duration_s);
    req->send(200, "application/json", body);
}

void httpapi_begin(void)
{
    if (started) return;
    started = true;

    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[http] http://%s.local/\n", MDNS_HOSTNAME);
    } else {
        Serial.println("[http] mDNS failed to start");
    }

    server.on("/health", HTTP_GET, handle_health);

    server.on("/emotion", HTTP_POST,
              [](AsyncWebServerRequest *req) { /* completion handled in body cb */ },
              nullptr,
              handle_emotion_body);

    server.onNotFound([](AsyncWebServerRequest *req) {
        send_err(req, 404, "try POST /emotion or GET /health");
    });

    server.begin();
    Serial.printf("[http] listening on :%d\n", HTTP_PORT);
}
