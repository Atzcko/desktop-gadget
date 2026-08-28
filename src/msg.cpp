#include "msg.h"
#include "settings.h"
#include "emotion.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>

static MsgPeer  contacts[MSG_PEERS_N];
static int      contact_n;
static MsgEntry history[MSG_HISTORY_N];
static int      history_n;
static volatile uint32_t rev_v;
static volatile int      unread_v;
static volatile bool     busy;
static volatile int      send_result = 0;

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

enum CmdType { CMD_NONE, CMD_SCAN, CMD_SEND };
static struct { CmdType type; char ip[16]; char text[MSG_TEXT_MAX + 1]; } cmd;
static QueueHandle_t cmd_q;

const char *msg_name(void) { return settings_get().ble_name; }

/* ---------------------------------------------------------------- contacts */

/* Learn or refresh a name->ip pair. Names are the identity (D046): a known
 * name arriving from a new IP means the device moved; take the new address. */
static void contact_learn_locked(const char *name, const char *ip)
{
    if (!name || !name[0] || !ip || !ip[0]) return;
    for (int i = 0; i < contact_n; i++) {
        if (strcasecmp(contacts[i].name, name) == 0) {
            snprintf(contacts[i].ip, sizeof(contacts[i].ip), "%s", ip);
            return;
        }
    }
    if (contact_n >= MSG_PEERS_N) {          /* forget the oldest slot */
        for (int i = 1; i < contact_n; i++) contacts[i - 1] = contacts[i];
        contact_n--;
    }
    snprintf(contacts[contact_n].name, sizeof(contacts[0].name), "%s", name);
    snprintf(contacts[contact_n].ip,   sizeof(contacts[0].ip),   "%s", ip);
    contact_n++;
}

int  msg_contact_count(void) { return contact_n; }
bool msg_contact(int i, MsgPeer *out)
{
    if (i < 0 || i >= contact_n) return false;
    taskENTER_CRITICAL(&mux);
    *out = contacts[i];
    taskEXIT_CRITICAL(&mux);
    return true;
}

bool msg_resolve(const char *q, char *ip_out, size_t cap)
{
    if (!q || !q[0]) return false;
    taskENTER_CRITICAL(&mux);
    for (int i = 0; i < contact_n; i++) {
        if (strcasecmp(contacts[i].name, q) == 0) {
            snprintf(ip_out, cap, "%s", contacts[i].ip);
            taskEXIT_CRITICAL(&mux);
            return true;
        }
    }
    taskEXIT_CRITICAL(&mux);

    /* Not a known name: accept a literal dotted quad — "a unique number". */
    int a, b, c, d;
    if (sscanf(q, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
        !(a | b | c | d & ~255)) {          /* all 0-255 */
        snprintf(ip_out, cap, "%d.%d.%d.%d", a, b, c, d);
        return true;
    }
    return false;
}

/* ----------------------------------------------------------------- history */

static void history_add_locked(const char *peer, const char *text, bool out)
{
    if (history_n < MSG_HISTORY_N) history_n++;
    for (int i = history_n - 1; i > 0; i--) history[i] = history[i - 1];
    snprintf(history[0].peer, sizeof(history[0].peer), "%s", peer);
    snprintf(history[0].text, sizeof(history[0].text), "%s", text);
    history[0].at_ms   = millis();
    history[0].outgoing = out;
    rev_v++;
}

uint32_t msg_rev(void)           { return rev_v; }
int      msg_history_count(void) { return history_n; }
bool     msg_history(int i, MsgEntry *out)
{
    if (i < 0 || i >= history_n) return false;
    taskENTER_CRITICAL(&mux);
    *out = history[i];
    taskEXIT_CRITICAL(&mux);
    return true;
}

int  msg_unread(void)    { return unread_v; }
void msg_mark_read(void) { unread_v = 0; }

void msg_store(const char *from, const char *from_ip, const char *text)
{
    taskENTER_CRITICAL(&mux);
    history_add_locked(from, text, false);
    contact_learn_locked(from, from_ip);     /* reply needs no scan */
    taskEXIT_CRITICAL(&mux);
    unread_v = unread_v + 1;

    int st = emotion_from_name("excited");
    if (st >= 0) {
        EmotionRequest r = {};
        r.state      = (uint8_t)st;
        r.duration_s = 10;
        snprintf(r.message, sizeof(r.message), "%s: %.32s", from, text);
        emotion_post(r);
    }
    Serial.printf("[msg] from %s (%s): %s\n", from, from_ip ? from_ip : "?", text);
}

void msg_note_sent(const char *peer, const char *ip, const char *text)
{
    taskENTER_CRITICAL(&mux);
    history_add_locked(peer, text, true);
    contact_learn_locked(peer, ip);
    taskEXIT_CRITICAL(&mux);
}

/* ------------------------------------------------------------------ worker */

static void do_scan(void)
{
    int n = MDNS.queryService("gadget-msg", "tcp");
    const String self_ip = WiFi.localIP().toString();
    taskENTER_CRITICAL(&mux);
    for (int i = 0; i < n; i++) {
        String ip = MDNS.IP(i).toString();
        if (ip == self_ip || ip == "0.0.0.0") continue;
        String name = MDNS.txt(i, "name");
        if (name.length() == 0) name = MDNS.hostname(i);
        contact_learn_locked(name.c_str(), ip.c_str());
    }
    taskEXIT_CRITICAL(&mux);
    rev_v++;                                 /* the UI repaints its list */
    Serial.printf("[msg] scan merged %d service(s), %d contact(s)\n", n, contact_n);
}

static void do_send(const char *ip, const char *text)
{
    char esc[MSG_TEXT_MAX * 2 + 2], name_esc[MSG_FROM_MAX * 2 + 2];
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

bool msg_busy(void)             { return busy; }
int  msg_last_send_result(void) { return send_result; }

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
