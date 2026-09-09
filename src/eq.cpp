/**
 * eq.cpp — spectrum worker. See D058 and eq.h.
 *
 * One task: GET /eq/stream from the companion and read it line by line.
 * A line is 64 hex chars (32 bands × 0–255); a line starting with "!" is a
 * status to display. The stream is ~2 KB/s, so a byte-at-a-time read is
 * fine. If the companion ends the response (its helper died — usually the
 * one-time permission), we wait two seconds and reconnect, which is what
 * makes granting the permission self-healing from the clock's side.
 */
#include "eq.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

extern const char *yt_play_host(void);

#define EQ_PORT 8999

static volatile bool     stop_flag, streaming_v, companion_said;
static volatile uint32_t rev_v;
static uint8_t           levels[EQ_BANDS];
static char              status_buf[72];
static TaskHandle_t      task_h;
static portMUX_TYPE      mux = portMUX_INITIALIZER_UNLOCKED;

static int hexv(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void handle_line(const char *ln, size_t n)
{
    if (n == 0) return;
    if (ln[0] == '!') {
        snprintf(status_buf, sizeof(status_buf), "%.*s", (int)(n - 1), ln + 1);
        companion_said = true;               /* keep it through reconnects */
        streaming_v = false;
        rev_v++;
        return;
    }
    if (n < EQ_BANDS * 2) return;
    uint8_t tmp[EQ_BANDS];
    for (int i = 0; i < EQ_BANDS; i++) {
        const int a = hexv(ln[i * 2]), b = hexv(ln[i * 2 + 1]);
        if (a < 0 || b < 0) return;
        tmp[i] = (uint8_t)(a * 16 + b);
    }
    taskENTER_CRITICAL(&mux);
    memcpy(levels, tmp, EQ_BANDS);
    taskEXIT_CRITICAL(&mux);
    if (!streaming_v) { streaming_v = true; status_buf[0] = '\0'; companion_said = false; }
    rev_v++;
}

static void eq_task(void *)
{
    char   line[160];
    size_t n = 0;
    while (!stop_flag) {
        const char *host = yt_play_host();
        if (!host || !host[0]) {
            snprintf(status_buf, sizeof(status_buf), "no companion IP - set it in YouTube > Setup");
            rev_v++;
            vTaskDelay(pdMS_TO_TICKS(800));
            continue;
        }
        char url[80];
        snprintf(url, sizeof(url), "http://%s:%d/eq/stream", host, EQ_PORT);
        HTTPClient http;
        http.setConnectTimeout(4000);
        http.setTimeout(20000);
        if (!http.begin(url)) { vTaskDelay(pdMS_TO_TICKS(800)); continue; }
        const int code = http.GET();
        if (code != 200) {
            snprintf(status_buf, sizeof(status_buf), "companion HTTP %d - is tools/ytserve running?", code);
            rev_v++;
            http.end();
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }
        WiFiClient *s = http.getStreamPtr();
        uint32_t last = millis();
        n = 0;
        if (!companion_said) { snprintf(status_buf, sizeof(status_buf), "listening for sound..."); rev_v++; }
        while (!stop_flag) {
            int avail = s->available();
            if (avail <= 0) {
                if (!s->connected() || millis() - last > 15000) break;
                vTaskDelay(pdMS_TO_TICKS(2));
                continue;
            }
            while (avail-- > 0) {
                const int c = s->read();
                if (c < 0) break;
                last = millis();
                if (c == '\n')                    { handle_line(line, n); n = 0; }
                else if (n < sizeof(line) - 1)    line[n++] = (char)c;
                else                              n = 0;          /* garbage: resync */
            }
        }
        http.end();
        streaming_v = false;
        if (!stop_flag) vTaskDelay(pdMS_TO_TICKS(2000));   /* companion ended it: retry */
    }
    task_h = nullptr;
    vTaskDelete(nullptr);
}

bool eq_start(void)
{
    if (task_h) return false;
    stop_flag = false; streaming_v = false; companion_said = false; rev_v = 0;
    memset(levels, 0, sizeof(levels));
    snprintf(status_buf, sizeof(status_buf), "connecting to the companion...");
    xTaskCreate(eq_task, "eq", 8192, nullptr, 3, &task_h);
    return true;
}

void eq_stop(void)
{
    stop_flag = true;
    for (int i = 0; i < 40 && task_h; i++) vTaskDelay(pdMS_TO_TICKS(15));
    streaming_v = false;
}

bool        eq_streaming(void) { return streaming_v; }
const char *eq_status(void)    { return status_buf; }
uint32_t    eq_rev(void)       { return rev_v; }

void eq_levels(uint8_t out[EQ_BANDS])
{
    taskENTER_CRITICAL(&mux);
    memcpy(out, levels, EQ_BANDS);
    taskEXIT_CRITICAL(&mux);
}
