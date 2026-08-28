#include "httpapi.h"
#include "emotion.h"
#include "settings.h"
#include "app_host.h"
#include "gauge.h"
#include "msg.h"
#include "theme.h"
#include "yt.h"
#include <LittleFS.h>

extern "C" void games_request_open_c(int);
#include "app.h"
#include "app.h"
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
#include <esp_system.h>
#include <esp_core_dump.h>
#include <esp_ota_ops.h>

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
    const BatteryState bat = app_battery();
    NetStatus   st = net_status();
    WeatherData w  = net_weather();
    Settings   &s  = settings_get();

    uint32_t a = net_weather_age_s();
    long age = (a == UINT32_MAX) ? -1 : (long)a;

    char body[768];
    snprintf(body, sizeof(body),
             "{\"ok\":true,"
             "\"version\":\"%s\","
             "\"build\":\"%s\","
             "\"git\":\"%s\","
             "\"current\":\"%s\","
             "\"rotation\":%u,"
             "\"panel\":\"%dx%d\","
             "\"theme\":\"%s\","
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
             "\"battery\":{\"present\":%s,\"mv\":%u,\"pct\":%d,\"charging\":%s,\"vbus\":%s,"
             "\"gauge\":\"hybrid\",\"ma\":%d,\"mah_used\":%d},"
             "\"brightness\":{\"day\":%u,\"night\":%u,\"follows_sun\":%s},"
             "\"city\":\"%s\","
             "\"emotion\":{\"active\":%s,\"state\":\"%s\",\"remaining_s\":%u},"
             "\"ble\":{\"running\":%s,\"connected\":%s,\"name\":\"%s\","
             "\"subs\":{\"mouse\":%d,\"key\":%d}},"
             "\"free_psram\":%u,"
             "\"free_heap\":%u}",
             FW_VERSION,
             FW_BUILD,
             FW_GIT,
             app_host_current(),
             (unsigned)(settings_get().rotation * 90u),
             (int)lv_disp_get_hor_res(nullptr), (int)lv_disp_get_ver_res(nullptr),
             theme_get().name,
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
             bat.present  ? "true" : "false",
             (unsigned)bat.mv,
             bat.pct,
             bat.charging ? "true" : "false",
             bat.vbus     ? "true" : "false",
             gauge_ma(),
             gauge_mah_used(),
             s.brightness_day, s.brightness_night,
             s.night_follows_sun ? "true" : "false",
             s.city,
             emotion_active() ? "true" : "false",
             emotion_name(emotion_current_state()),
             (unsigned)emotion_remaining_s(),
             ble_is_running()   ? "true" : "false",
             ble_is_connected() ? "true" : "false",
             s.ble_name,
             ble_mouse_subs(),
             ble_key_subs(),
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

/* ---------------------------------------------------------------- launch -- */
/*
 * POST /launch {"name":"Blink"} — open a screen without a finger. See D039.
 *
 * The switch itself happens on the LVGL loop; this only records the request,
 * because touching LVGL from the web server's task is the one rule this
 * firmware has never broken (D018).
 */
static void handle_launch_body(AsyncWebServerRequest *req, uint8_t *data,
                               size_t len, size_t index, size_t total)
{
    static String buf;
    if (index == 0) buf = "";
    buf.concat((const char *)data, len);
    if (index + len != total) return;

    char name[24] = "";
    const char *p = strstr(buf.c_str(), "\"name\"");
    if (p) { p = strchr(p + 6, '"'); if (p) sscanf(p + 1, "%23[^\"]", name); }
    if (!name[0]) { send_err(req, 400, "expected {\"name\":\"...\"}"); return; }

    if (!app_host_request_open(name)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "no app named '%s' (try clock, drawer, or GET /apps)", name);
        send_err(req, 404, msg);
        return;
    }
    char body[96];
    snprintf(body, sizeof(body), "{\"ok\":true,\"opening\":\"%s\"}", name);
    req->send(200, "application/json", body);
}

/* -------------------------------------------------------------- messages -- */
/*
 * POST /msg — how another gadget's message ARRIVES (D046). Also the remote
 * test seam: curl can play the role of a second device.
 * GET /messages — the inbox, for the app and for remote verification.
 * POST /send {"ip":"...","text":"..."} — ask THIS device to send; used by
 * tests and by anything on the LAN that wants to relay through the clock.
 */
static void handle_msg_body(AsyncWebServerRequest *req, uint8_t *data,
                            size_t len, size_t index, size_t total)
{
    static String buf;
    if (index == 0) buf = "";
    buf.concat((const char *)data, len);
    if (index + len != total) return;

    char from[MSG_FROM_MAX + 1] = "", text[MSG_TEXT_MAX + 1] = "";
    const char *p = strstr(buf.c_str(), "\"from\"");
    if (p) { p = strchr(p + 6, '"'); if (p) sscanf(p + 1, "%23[^\"]", from); }
    p = strstr(buf.c_str(), "\"text\"");
    if (p) { p = strchr(p + 6, '"'); if (p) sscanf(p + 1, "%96[^\"]", text); }

    if (!from[0] || !text[0]) {
        send_err(req, 400, "expected {\"from\":\"...\",\"text\":\"...\"}");
        return;
    }
    msg_store(from, req->client()->remoteIP().toString().c_str(), text);
    req->send(200, "application/json", "{\"ok\":true,\"delivered\":true}");
}

static void handle_messages(AsyncWebServerRequest *req)
{
    char body[1600];
    size_t o = snprintf(body, sizeof(body),
                        "{\"ok\":true,\"name\":\"%s\",\"unread\":%d,\"history\":[",
                        msg_name(), msg_unread());
    MsgEntry e;
    for (int i = 0; i < msg_history_count() && o + 180 < sizeof(body); i++) {
        if (!msg_history(i, &e)) break;
        char f[48], t[200];
        json_escape(e.peer, f, sizeof(f));
        json_escape(e.text, t, sizeof(t));
        o += snprintf(body + o, sizeof(body) - o,
                      "%s{\"peer\":\"%s\",\"dir\":\"%s\",\"text\":\"%s\",\"age_s\":%lu}",
                      i ? "," : "", f, e.outgoing ? "out" : "in", t,
                      (unsigned long)((millis() - e.at_ms) / 1000UL));
    }
    snprintf(body + o, sizeof(body) - o, "]}");
    req->send(200, "application/json", body);
}

static void handle_send_body(AsyncWebServerRequest *req, uint8_t *data,
                             size_t len, size_t index, size_t total)
{
    static String buf;
    if (index == 0) buf = "";
    buf.concat((const char *)data, len);
    if (index + len != total) return;

    char ip[16] = "", text[MSG_TEXT_MAX + 1] = "";
    const char *p = strstr(buf.c_str(), "\"ip\"");
    if (p) { p = strchr(p + 4, '"'); if (p) sscanf(p + 1, "%15[^\"]", ip); }
    p = strstr(buf.c_str(), "\"text\"");
    if (p) { p = strchr(p + 6, '"'); if (p) sscanf(p + 1, "%96[^\"]", text); }

    if (!ip[0] || !text[0]) { send_err(req, 400, "expected {\"ip\",\"text\"}"); return; }
    if (msg_busy())         { send_err(req, 503, "sender busy"); return; }
    msg_request_send(ip, text);
    req->send(200, "application/json", "{\"ok\":true,\"sending\":true}");
}

/* ---------------------------------------------------------------- rotate -- */
/*
 * POST /rotate {"deg":0|90|180|270} — the BOOT button, callable from a shell.
 * Exists for the same reason as /launch (D039): I cannot press the button,
 * and an orientation feature nobody can verify remotely is a checklist item,
 * not a feature. Served on the LVGL loop via app_request_rotation().
 */
static void handle_rotate_body(AsyncWebServerRequest *req, uint8_t *data,
                               size_t len, size_t index, size_t total)
{
    static String buf;
    if (index == 0) buf = "";
    buf.concat((const char *)data, len);
    if (index + len != total) return;

    int deg = -1;
    const char *p = strstr(buf.c_str(), "\"deg\"");
    if (p) { p = strchr(p + 5, ':'); if (p) deg = atoi(p + 1); }
    if (deg != 0 && deg != 90 && deg != 180 && deg != 270) {
        send_err(req, 400, "expected {\"deg\":0|90|180|270}");
        return;
    }
    app_request_rotation((uint8_t)(deg / 90));
    char body[64];
    snprintf(body, sizeof(body), "{\"ok\":true,\"rotating_to\":%d}", deg);
    req->send(200, "application/json", body);
}

/* ----------------------------------------------------------------- crash -- */
/*
 * GET /crash — the last panic, read back over Wi-Fi. See D038.
 *
 * The cable stopped being attached once OTA landed, which also removed the
 * serial backtrace — the one tool that turns "it restarts" into a fix. The
 * partition table has carried a `coredump` partition since Stage 0 and
 * arduino-esp32 ships with CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y, so the
 * panic handler has been writing full ELF core dumps to flash all along.
 * Nothing needed enabling; it needed reading.
 *
 * Decode the PCs with:
 *   xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/t4s3/firmware.elf <pc...>
 * or just run tools/crash, which does it for you.
 */
static const char *reset_reason_name(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "poweron";
    case ESP_RST_EXT:      return "external";
    case ESP_RST_SW:       return "software";      /* our own ESP.restart(), e.g. OTA */
    case ESP_RST_PANIC:    return "panic";         /* <-- the interesting one */
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO:     return "sdio";
    case ESP_RST_DEEPSLEEP:return "deepsleep";
    default:               return "unknown";
    }
}

static void handle_crash(AsyncWebServerRequest *req)
{
    char body[1024];
    size_t o = snprintf(body, sizeof(body),
                        "{\"ok\":true,\"reset_reason\":\"%s\"", reset_reason_name());

    if (esp_core_dump_image_check() != ESP_OK) {
        snprintf(body + o, sizeof(body) - o, ",\"core_dump\":false}");
        req->send(200, "application/json", body);
        return;
    }

    /* ~500 bytes; the async task's stack is not the place for it. */
    esp_core_dump_summary_t *s =
        (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
    if (!s) { send_err(req, 500, "out of memory reading the core dump"); return; }

    if (esp_core_dump_get_summary(s) != ESP_OK) {
        free(s);
        snprintf(body + o, sizeof(body) - o, ",\"core_dump\":\"unreadable\"}");
        req->send(200, "application/json", body);
        return;
    }

    /*
     * Whether the dump can be trusted against the ELF on disk is a FACT the
     * device can report, not a warning a human has to remember. The dump
     * carries the first 16 hex chars of the crashing image's ELF sha256;
     * compare it with the running image's and say plainly whether they match.
     * They will not, whenever a rebuild has happened since the crash — which
     * is exactly when the decoded line numbers become fiction.
     */
    char running_sha[APP_ELF_SHA256_SZ] = "";
    const esp_app_desc_t *desc = esp_ota_get_app_description();
    if (desc)
        for (int i = 0; i < (APP_ELF_SHA256_SZ - 1) / 2; i++)
            snprintf(running_sha + i * 2, 3, "%02x", desc->app_elf_sha256[i]);

    const bool same_build = (strncmp(running_sha, (const char *)s->app_elf_sha256,
                                     APP_ELF_SHA256_SZ - 1) == 0);

    char task[24];
    json_escape(s->exc_task, task, sizeof(task));
    o += snprintf(body + o, sizeof(body) - o,
                  ",\"core_dump\":true,\"task\":\"%s\",\"pc\":\"0x%08x\""
                  ",\"exc_cause\":%u,\"exc_vaddr\":\"0x%08x\""
                  ",\"crash_elf\":\"%s\",\"running_elf\":\"%s\",\"same_build\":%s"
                  ",\"corrupted\":%s,\"backtrace\":[",
                  task, (unsigned)s->exc_pc,
                  (unsigned)s->ex_info.exc_cause, (unsigned)s->ex_info.exc_vaddr,
                  (const char *)s->app_elf_sha256, running_sha,
                  same_build ? "true" : "false",
                  s->exc_bt_info.corrupted ? "true" : "false");

    for (uint32_t i = 0; i < s->exc_bt_info.depth && i < 16 && o + 16 < sizeof(body); i++)
        o += snprintf(body + o, sizeof(body) - o, i ? ",\"0x%08x\"" : "\"0x%08x\"",
                      (unsigned)s->exc_bt_info.bt[i]);

    snprintf(body + o, sizeof(body) - o, "]}");
    free(s);
    req->send(200, "application/json", body);
}

static void handle_crash_clear(AsyncWebServerRequest *req)
{
    esp_core_dump_image_erase();
    req->send(200, "application/json", "{\"ok\":true,\"cleared\":true}");
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
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"deleted\":\"%s\",\"visible_after\":\"the clock screen\"}",
             name.c_str());
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
                 "{\"ok\":true,\"saved\":\"%s\",\"bytes\":%u,"
                 "\"visible_after\":\"the clock screen\"}",
                 name.c_str(), (unsigned)up_len);
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
        /* The messaging identity (D046): the service other gadgets browse
         * for, carrying this device's name. Two clocks on one LAN should get
         * different names in Settings > BLE - the name IS the identity. */
        MDNS.addService("gadget-msg", "tcp", HTTP_PORT);
        MDNS.addServiceTxt("gadget-msg", "tcp", "name", msg_name());
        Serial.printf("[http] http://%s.local/\n", MDNS_HOSTNAME);
    } else {
        Serial.println("[http] mDNS failed to start");
    }

    server.on("/health", HTTP_GET, handle_health);

    server.on("/update", HTTP_POST, handle_update_done, nullptr, handle_update_body);

    server.on("/launch", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr, handle_launch_body);

    server.on("/youtube", HTTP_GET, [](AsyncWebServerRequest *req) {
        char chans[220] = "";
        for (int i = 0; i < yt_channel_count(); i++) {
            strlcat(chans, i ? "," : "", sizeof(chans));
            strlcat(chans, yt_channel(i), sizeof(chans));
        }
        char body[420], st[96];
        json_escape(yt_status(), st, sizeof(st));
        snprintf(body, sizeof(body),
                 "{\"ok\":true,\"has_key\":%s,\"mode\":%d,\"streaming\":%s,\"frames\":%lu,\"channels\":\"%s\","
                 "\"host\":\"%s\",\"videos\":%d,\"busy\":%s,\"status\":\"%s\"}",
                 yt_has_key() ? "true" : "false", yt_mode(),
                 yt_streaming() ? "true" : "false",
                 (unsigned long)yt_frame_rev(), chans, yt_play_host(),
                 yt_video_count(), yt_busy() ? "true" : "false", st);
        req->send(200, "application/json", body);
    });

    server.on("/youtube", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr,
              [](AsyncWebServerRequest *req, uint8_t *data, size_t len,
                 size_t index, size_t total) {
        static String buf;
        if (index == 0) buf = "";
        buf.concat((const char *)data, len);
        if (index + len != total) return;
        char val[220];
        bool did = false;
        const char *p;
        if ((p = strstr(buf.c_str(), "\"key\""))) {
            p = strchr(p + 5, '"');
            if (p && sscanf(p + 1, "%47[^\"]", val) == 1) { yt_set_key(val); did = true; }
        }
        if ((p = strstr(buf.c_str(), "\"channels\""))) {
            p = strchr(p + 10, '"');
            if (p && sscanf(p + 1, "%219[^\"]", val) == 1) { yt_set_channels(val); did = true; }
        }
        if ((p = strstr(buf.c_str(), "\"host\""))) {
            p = strchr(p + 6, '"');
            if (p && sscanf(p + 1, "%19[^\"]", val) == 1) { yt_set_play_host(val); did = true; }
        }
        if ((p = strstr(buf.c_str(), "\"region\""))) {
            p = strchr(p + 8, '"');
            if (p && sscanf(p + 1, "%3[^\"]", val) == 1) { yt_set_region(val); did = true; }
        }
        if ((p = strstr(buf.c_str(), "\"mode\""))) {
            p = strchr(p + 6, ':');
            if (p) { yt_set_mode(atoi(p + 1)); did = true; }
        }
        if ((p = strstr(buf.c_str(), "\"search\""))) {
            p = strchr(p + 8, '"');
            if (p && sscanf(p + 1, "%63[^\"]", val) == 1) { yt_set_search(val); did = true; }
        }
        if ((p = strstr(buf.c_str(), "\"play_here\""))) {
            p = strchr(p + 11, '"');
            if (p && sscanf(p + 1, "%15[^\"]", val) == 1) {
                yt_stream_start(val);
                req->send(200, "application/json", "{\"ok\":true,\"streaming\":true}");
                return;
            }
        }
        if (strstr(buf.c_str(), "\"stop\"")) {
            yt_stream_stop();
            req->send(200, "application/json", "{\"ok\":true,\"stopped\":true}");
            return;
        }
        if (!did) { send_err(req, 400, "expected key / channels / host"); return; }
        yt_request_refresh();
        req->send(200, "application/json", "{\"ok\":true,\"refreshing\":true}");
    });

    server.on("/theme", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr,
              [](AsyncWebServerRequest *req, uint8_t *data, size_t len,
                 size_t index, size_t total) {
        static String buf;
        if (index == 0) buf = "";
        buf.concat((const char *)data, len);
        if (index + len != total) return;
        int n = -1;
        const char *p = strstr(buf.c_str(), "\"n\"");
        if (p) { p = strchr(p + 3, ':'); if (p) n = atoi(p + 1); }
        if (n < 0 || n >= theme_count()) { send_err(req, 400, "expected {\"n\":0..}"); return; }
        settings_get().theme = (uint8_t)n;
        settings_save();
        app_request_rebuild();               /* same rotation, new colors */
        char body[64];
        snprintf(body, sizeof(body), "{\"ok\":true,\"theme\":\"%s\"}", theme_at(n).name);
        req->send(200, "application/json", body);
    });

    /* ROM cartridge for the Game Boy (D056): raw .gb body -> LittleFS.
     * Homebrew/owned ROMs only - same posture as every upload here. */
    /* The lesson of 2026-08-28: a device that ships its own updates but has
     * no remote reboot can wedge into a state only a finger can clear. */
    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *req) {
        req->onDisconnect([]() { ESP.restart(); });
        req->send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
    });

    server.on("/games", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr,
              [](AsyncWebServerRequest *req, uint8_t *data, size_t len,
                 size_t index, size_t total) {
        static String buf;
        if (index == 0) buf = "";
        buf.concat((const char *)data, len);
        if (index + len != total) return;
        int w = -1;
        if      (strstr(buf.c_str(), "snake"))    w = 0;
        else if (strstr(buf.c_str(), "breakout")) w = 1;
        else if (strstr(buf.c_str(), "jump"))     w = 2;
        else if (strstr(buf.c_str(), "gameboy"))  w = 3;
        if (w < 0) { send_err(req, 400, "expected snake|breakout|gameboy"); return; }
        games_request_open_c(w);
        req->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/rom", HTTP_DELETE, [](AsyncWebServerRequest *req) {
        if (!req->hasParam("name")) { send_err(req, 400, "need ?name="); return; }
        String nm = req->getParam("name")->value();
        nm.replace("/", "");
        bool ok = LittleFS.remove("/roms/" + nm);
        String sav = "/roms/" + nm;
        sav.replace(".gb", ".sav");
        LittleFS.remove(sav);
        req->send(ok ? 200 : 404, "application/json",
                  ok ? "{\"ok\":true,\"removed\":true}" : "{\"ok\":false}");
    });

    server.on("/rom", HTTP_POST,
              [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", "{\"ok\":true,\"cartridge\":\"loaded\"}");
    }, nullptr,
              [](AsyncWebServerRequest *req, uint8_t *data, size_t len,
                 size_t index, size_t total) {
        static File rf;
        if (index == 0) {
            LittleFS.mkdir("/roms");
            char path[48] = "/roms/boot.gb";
            if (req->hasParam("name")) {
                String nm = req->getParam("name")->value();
                nm.replace("/", "");             /* stay inside /roms */
                if (nm.endsWith(".gb") && nm.length() < 32)
                    snprintf(path, sizeof(path), "/roms/%s", nm.c_str());
            }
            rf = LittleFS.open(path, "w");
            String sav = String(path);
            sav.replace(".gb", ".sav");
            LittleFS.remove(sav);                /* new cart, fresh save  */
        }
        if (rf) rf.write(data, len);
        if (index + len == total && rf) { rf.close(); Serial.printf("[gb] cart %u bytes\n", (unsigned)total); }
    });

    server.on("/msg", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr, handle_msg_body);
    server.on("/messages", HTTP_GET, handle_messages);
    server.on("/send", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr, handle_send_body);

    server.on("/rotate", HTTP_POST,
              [](AsyncWebServerRequest *) {}, nullptr, handle_rotate_body);

    server.on("/crash", HTTP_GET,    handle_crash);
    server.on("/crash", HTTP_DELETE, handle_crash_clear);

    server.on("/apps", HTTP_GET,    handle_apps_list);
    server.on("/apps", HTTP_DELETE, handle_apps_delete);
    server.on("/apps", HTTP_POST,   handle_apps_post, nullptr, handle_apps_body);

    server.on("/emotion", HTTP_POST,
              [](AsyncWebServerRequest *req) { /* completion handled in body cb */ },
              nullptr,
              handle_emotion_body);

    server.onNotFound([](AsyncWebServerRequest *req) {
        send_err(req, 404, "try GET /health, GET /crash, POST /emotion, GET|POST|DELETE /apps, POST /update");
    });

    server.begin();
    Serial.printf("[http] listening on :%d\n", HTTP_PORT);
}
