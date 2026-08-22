#include "httpapi.h"
#include "emotion.h"
#include "settings.h"
#include "net.h"
#include "version.h"
#include "config.h"
#include "ble.h"
#include "script.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_heap_caps.h>

static AsyncWebServer server(HTTP_PORT);
static bool started;

/* The message may quote user input, so it cannot be trusted to be
 * JSON-safe. Escape rather than hope. */
static void json_escape(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    for (const char *p = in; *p && o + 2 < cap; p++) {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = *p; }
        else if ((unsigned char)*p < 0x20) { out[o++] = ' '; }
        else out[o++] = *p;
    }
    out[o] = '\0';
}

static void send_err(AsyncWebServerRequest *req, int code, const char *msg)
{
    char safe[192];
    json_escape(msg, safe, sizeof(safe));
    char body[224];
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", safe);
    req->send(code, "application/json", body);
}

static void handle_health(AsyncWebServerRequest *req)
{
    NetStatus   st = net_status();
    WeatherData w  = net_weather();
    Settings   &s  = settings_get();

    uint32_t a = net_weather_age_s();
    long age = (a == UINT32_MAX) ? -1 : (long)a;

    char body[640];
    snprintf(body, sizeof(body),
             "{\"ok\":true,"
             "\"version\":\"%s\","
             "\"build\":\"%s\","
             "\"git\":\"%s\","
             "\"uptime_s\":%lu,"
             "\"rssi\":%d,"
             "\"ip\":\"%s\","
             "\"ssid\":\"%s\","
             "\"time_synced\":%s,"
             "\"weather_age_s\":%ld,"
             "\"weather_valid\":%s,"
             "\"temp_c\":%.1f,"
             "\"humidity_pct\":%.0f,"
             "\"weather_code\":%d,"
             "\"is_day\":%s,"
             "\"brightness\":{\"day\":%u,\"night\":%u,\"follows_sun\":%s},"
             "\"city\":\"%s\","
             "\"emotion\":{\"active\":%s,\"state\":\"%s\",\"remaining_s\":%u},"
             "\"ble\":{\"running\":%s,\"connected\":%s,\"name\":\"%s\"},"
             "\"free_psram\":%u,"
             "\"free_heap\":%u}",
             FW_VERSION,
             FW_BUILD,
             FW_GIT,
             (unsigned long)(millis() / 1000UL),
             st.rssi,
             st.wifi_up ? st.ip : "",
             s.wifi_ssid,
             st.time_valid ? "true" : "false",
             age,
             w.valid ? "true" : "false",
             w.current,
             w.humidity,
             w.code,
             w.is_day ? "true" : "false",
             s.brightness_day, s.brightness_night,
             s.night_follows_sun ? "true" : "false",
             s.city,
             emotion_active() ? "true" : "false",
             emotion_name(emotion_current_state()),
             (unsigned)emotion_remaining_s(),
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

/* ------------------------------------------------------------------ apps -- */
/*
 * Script apps, added and removed without a reboot. See D037.
 *
 *   GET    /apps            list them, plus filesystem usage
 *   POST   /apps?name=foo   raw Lua body -> /apps/foo.lua
 *   DELETE /apps?name=foo
 *
 * The name is a query parameter rather than a path segment because
 * ESPAsyncWebServer matches paths literally — a wildcard route would need
 * onNotFound parsing, and the name has to be validated either way.
 */
static bool app_name_param(AsyncWebServerRequest *req, String &out)
{
    if (!req->hasParam("name")) return false;
    out = req->getParam("name")->value();
    return out.length() > 0;
}

static void handle_apps_list(AsyncWebServerRequest *req)
{
    char body[512];
    script_list_json(body, sizeof(body));
    req->send(200, "application/json", body);
}

static void handle_apps_delete(AsyncWebServerRequest *req)
{
    String name;
    if (!app_name_param(req, name)) { send_err(req, 400, "need ?name="); return; }
    if (!script_delete(name.c_str())) { send_err(req, 404, "no such script"); return; }

    Serial.printf("[apps] deleted %s\n", name.c_str());
    char body[128];
    snprintf(body, sizeof(body), "{\"ok\":true,\"deleted\":\"%s\",\"apps\":%d}",
             name.c_str(), script_count());
    req->send(200, "application/json", body);
}

/*
 * Accumulated into one PSRAM buffer before saving. The script has to be
 * compiled as a whole to be validated, and a rejected upload must not have
 * already overwritten a working script of the same name.
 */
static uint8_t *up_buf;
static size_t   up_len;

static void handle_apps_body(AsyncWebServerRequest *req, uint8_t *data,
                             size_t len, size_t index, size_t total)
{
    if (index == 0) {
        free(up_buf);
        up_buf = (uint8_t *)heap_caps_malloc(total + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        up_len = 0;
        if (!up_buf) return;                 /* completion handler reports it */
    }
    if (up_buf && up_len + len <= total) {
        memcpy(up_buf + up_len, data, len);
        up_len += len;
    }
}

static void handle_apps_post(AsyncWebServerRequest *req)
{
    String name;
    if (!app_name_param(req, name)) { send_err(req, 400, "need ?name="); goto done; }
    if (!up_buf)                    { send_err(req, 500, "out of PSRAM for the upload"); goto done; }

    {
        char err[192] = "";
        if (!script_save(name.c_str(), up_buf, up_len, err, sizeof(err))) {
            send_err(req, 400, err);
            goto done;
        }
        Serial.printf("[apps] saved %s (%u bytes)\n", name.c_str(), (unsigned)up_len);
        char body[160];
        snprintf(body, sizeof(body),
                 "{\"ok\":true,\"saved\":\"%s\",\"bytes\":%u,\"apps\":%d}",
                 name.c_str(), (unsigned)up_len, script_count());
        req->send(200, "application/json", body);
    }
done:
    free(up_buf);
    up_buf = nullptr;
    up_len = 0;
}

/* ------------------------------------------------------------------- OTA -- */
/*
 * POST /update with the raw firmware.bin as the body. See D034.
 *
 * Raw body rather than multipart: the client is curl in tools/ota, so the
 * exact size arrives up front for Update.begin(), and the first chunk's
 * magic-byte check rejects a wrong file before 1.7 MB of it has been sent.
 *
 * Runs on the async_tcp task. Flash writes are safe there; the display is
 * only told about progress through the emotion queue, which is the same
 * thread-safe door every other transport uses.
 */
static void ota_emote(const char *state, uint16_t secs, const char *msg)
{
    int st = emotion_from_name(state);
    if (st < 0) return;
    EmotionRequest r = {};
    r.state      = (uint8_t)st;
    r.duration_s = secs;
    snprintf(r.message, sizeof(r.message), "%s", msg);
    emotion_post(r);
}

static void handle_update_body(AsyncWebServerRequest *req, uint8_t *data,
                               size_t len, size_t index, size_t total)
{
    static size_t last_log;

    if (index == 0) {
        /* A push that arrives mid-push wins; the half-written slot was
         * garbage either way. */
        if (Update.isRunning()) Update.abort();
        last_log = 0;

        Serial.printf("[ota] start: %u bytes\n", (unsigned)total);
        char m[EMOTION_MSG_MAX + 1];
        snprintf(m, sizeof(m), "firmware, %u KB", (unsigned)(total / 1024));
        ota_emote("flashing", 120, m);

        if (!Update.begin(total)) {
            Serial.printf("[ota] begin failed: %s\n", Update.errorString());
            return;                    /* completion handler reports it */
        }
    }

    if (Update.isRunning()) {
        if (Update.write(data, len) != len) {
            Serial.printf("[ota] write failed at %u: %s\n",
                          (unsigned)index, Update.errorString());
            Update.abort();
        } else if (index + len - last_log >= 262144) {
            last_log = index + len;
            Serial.printf("[ota] %u / %u\n", (unsigned)last_log, (unsigned)total);
        }
    }

    if (index + len == total && Update.isRunning()) {
        if (Update.end(true)) Serial.println("[ota] image valid, awaiting disconnect");
        else Serial.printf("[ota] end failed: %s\n", Update.errorString());
    }
}

static void handle_update_done(AsyncWebServerRequest *req)
{
    if (Update.hasError() || !Update.isFinished()) {
        char msg[96];
        snprintf(msg, sizeof(msg), "update failed: %s", Update.errorString());
        Update.abort();                /* clean slate for the retry */
        ota_emote("error", 8, "OTA failed");
        send_err(req, 500, msg);
        return;
    }

    /* Reboot when the client hangs up — TCP itself confirms the 200 landed.
     * Restarting inside the handler would race the response out the door. */
    req->onDisconnect([]() {
        Serial.println("[ota] rebooting into the new image");
        ESP.restart();
    });
    req->send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
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

    server.on("/update", HTTP_POST, handle_update_done, nullptr, handle_update_body);

    server.on("/apps", HTTP_GET,    handle_apps_list);
    server.on("/apps", HTTP_DELETE, handle_apps_delete);
    server.on("/apps", HTTP_POST,   handle_apps_post, nullptr, handle_apps_body);

    server.on("/emotion", HTTP_POST,
              [](AsyncWebServerRequest *req) { /* completion handled in body cb */ },
              nullptr,
              handle_emotion_body);

    server.onNotFound([](AsyncWebServerRequest *req) {
        send_err(req, 404, "try GET /health, POST /emotion, GET|POST|DELETE /apps, POST /update");
    });

    server.begin();
    Serial.printf("[http] listening on :%d\n", HTTP_PORT);
}
