#include "msg.h"
#include "settings.h"
#include "emotion.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>

static MsgPeer  peers[MSG_PEERS_N];
static int      peer_n;
static MsgEntry inbox[MSG_INBOX_N];
static int      inbox_n;
static volatile uint32_t inbox_rev_v;
static volatile bool     busy;
static volatile int      send_result = 0;

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/* Worker commands. One slot is enough: the UI disables itself while busy. */
enum CmdType { CMD_NONE, CMD_SCAN, CMD_SEND };
static struct { CmdType type; char ip[16]; char text[MSG_TEXT_MAX + 1]; } cmd;
static QueueHandle_t cmd_q;

const char *msg_name(void) { return settings_get().ble_name; }

void msg_store(const char *from, const char *text)
{
    taskENTER_CRITICAL(&mux);
    if (inbox_n < MSG_INBOX_N) inbox_n++;
    for (int i = inbox_n - 1; i > 0; i--) inbox[i] = inbox[i - 1];
    snprintf(inbox[0].from, sizeof(inbox[0].from), "%s", from);
    snprintf(inbox[0].text, sizeof(inbox[0].text), "%s", text);
    inbox[0].at_ms = millis();
    inbox_rev_v++;
    taskEXIT_CRITICAL(&mux);

    /* Show it on the desk the moment it lands — through the queue, which is
     * the one legal door into LVGL from this task (D018). */
    int st = emotion_from_name("excited");
    if (st >= 0) {
        EmotionRequest r = {};
        r.state      = (uint8_t)st;
        r.duration_s = 12;
        snprintf(r.message, sizeof(r.message), "%s: %.32s", from, text);
        emotion_post(r);
    }
    Serial.printf("[msg] from %s: %s\n", from, text);
}

uint32_t msg_inbox_rev(void)  { return inbox_rev_v; }
int      msg_inbox_count(void){ return inbox_n; }
bool     msg_inbox(int i, MsgEntry *out)
{
    if (i < 0 || i >= inbox_n) return false;
    taskENTER_CRITICAL(&mux);
    *out = inbox[i];
    taskEXIT_CRITICAL(&mux);
    return true;
}

int  msg_peer_count(void) { return peer_n; }
bool msg_peer(int i, MsgPeer *out)
{
    if (i < 0 || i >= peer_n) return false;
    taskENTER_CRITICAL(&mux);
    *out = peers[i];
    taskEXIT_CRITICAL(&mux);
    return true;
}

bool msg_busy(void)            { return busy; }
int  msg_last_send_result(void){ return send_result; }

static void do_scan(void)
{
    /* queryService blocks ~3 s — precisely why this task exists. */
    int n = MDNS.queryService("gadget-msg", "tcp");
    MsgPeer found[MSG_PEERS_N];
    int fn = 0;
    const String self_ip = WiFi.localIP().toString();

    for (int i = 0; i < n && fn < MSG_PEERS_N; i++) {
        String ip = MDNS.IP(i).toString();
        if (ip == self_ip || ip == "0.0.0.0") continue;   /* not ourselves */
        String name = MDNS.txt(i, "name");
        if (name.length() == 0) name = MDNS.hostname(i);
        snprintf(found[fn].name, sizeof(found[fn].name), "%s", name.c_str());
        snprintf(found[fn].ip,   sizeof(found[fn].ip),   "%s", ip.c_str());
        fn++;
    }
    taskENTER_CRITICAL(&mux);
    memcpy(peers, found, sizeof(peers));
    peer_n = fn;
    taskEXIT_CRITICAL(&mux);
    Serial.printf("[msg] scan: %d gadget(s)\n", fn);
}

static void do_send(const char *ip, const char *text)
{
    char esc[MSG_TEXT_MAX * 2 + 2], name_esc[MSG_FROM_MAX * 2 + 2];
    /* Minimal JSON escape: backslash and quote. Same rule as httpapi. */
    auto escape = [](const char *in, char *out, size_t cap) {
        size_t o = 0;
        for (const char *p = in; *p && o + 2 < cap; p++) {
            if (*p == '"' || *p == '\\') out[o++] = '\\';
            out[o++] = *p;
        }
        out[o] = '\0';
    };
    escape(text, esc, sizeof(esc));
    escape(msg_name(), name_esc, sizeof(name_esc));

    char url[48], body[MSG_TEXT_MAX * 2 + 64];
    snprintf(url, sizeof(url), "http://%s/msg", ip);
    snprintf(body, sizeof(body), "{\"from\":\"%s\",\"text\":\"%s\"}", name_esc, esc);

    HTTPClient http;
    http.setConnectTimeout(3000);
    http.setTimeout(4000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST((uint8_t *)body, strlen(body));
    http.end();
    send_result = code;
    Serial.printf("[msg] send to %s -> %d\n", ip, code);
}

static void msg_task(void *)
{
    for (;;) {
        if (xQueueReceive(cmd_q, &cmd, portMAX_DELAY) != pdTRUE) continue;
        busy = true;
        if      (cmd.type == CMD_SCAN) do_scan();
        else if (cmd.type == CMD_SEND) do_send(cmd.ip, cmd.text);
        busy = false;
    }
}

void msg_request_scan(void)
{
    if (busy) return;
    decltype(cmd) c = {}; c.type = CMD_SCAN;
    xQueueSend(cmd_q, &c, 0);
}

void msg_request_send(const char *ip, const char *text)
{
    if (busy || !ip || !text) return;
    decltype(cmd) c = {}; c.type = CMD_SEND;
    snprintf(c.ip, sizeof(c.ip), "%s", ip);
    snprintf(c.text, sizeof(c.text), "%s", text);
    send_result = -1;
    xQueueSend(cmd_q, &c, 0);
}

void msg_begin(void)
{
    cmd_q = xQueueCreate(2, sizeof(cmd));
    xTaskCreatePinnedToCore(msg_task, "msg", 6144, nullptr, 1, nullptr, 0);
}
