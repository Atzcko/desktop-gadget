/**
 * browser.cpp — remote-browser worker. See D057 and browser.h.
 *
 * Two worker tasks, neither touching LVGL:
 *   stream_task  reads /browse/stream (endless concatenated JPEGs), decodes
 *                the LATEST complete frame into a PSRAM double buffer.
 *   input_task   drains a queue and POSTs each tap/scroll/key to the companion.
 * The decode path is yt.cpp's, verbatim in spirit: LVGL's TJpgDec emits
 * RGB888, we pack to 565 and pre-swap for LV_COLOR_16_SWAP.
 */
#include "browser.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#include "src/extra/libs/sjpg/tjpgd.h"

extern const char *yt_play_host(void);      /* seed the host from YouTube's */

#define BROWSER_PORT 8999

static char          host[40];
static Preferences   bprefs;

static uint16_t     *fb[2];
static volatile uint8_t  fb_front;
static volatile uint32_t frame_rev_v;
static int           fbw, fbh;
static volatile bool stop_flag, streaming_v;
static char          status_buf[48];
static TaskHandle_t  stream_h, input_h;
static QueueHandle_t input_q;

struct BEvent { char path[24]; char body[192]; };

/* ------------------------------------------------ TJpgDec -> RGB565 (yt) -- */
struct JpgCtx { const uint8_t *data; size_t len, pos; uint16_t *out; int ow, oh; };

static size_t jpg_in(JDEC *jd, uint8_t *buf, size_t n)
{
    JpgCtx *c = (JpgCtx *)jd->device;
    if (c->pos + n > c->len) n = c->len - c->pos;
    if (buf) memcpy(buf, c->data + c->pos, n);
    c->pos += n;
    return n;
}

static int jpg_out(JDEC *jd, void *bitmap, JRECT *r)
{
    JpgCtx *c = (JpgCtx *)jd->device;
    const uint8_t *p = (const uint8_t *)bitmap;
    for (int y = r->top; y <= r->bottom; y++) {
        for (int x = r->left; x <= r->right; x++) {
            if (x < c->ow && y < c->oh) {
                uint16_t px = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3);
#if LV_COLOR_16_SWAP
                px = (uint16_t)((px >> 8) | (px << 8));
#endif
                c->out[y * c->ow + x] = px;
            }
            p += 3;
        }
    }
    return 1;
}

static bool decode_frame(const uint8_t *jpg, size_t len)
{
    static uint8_t work[8192];              /* stream task only; wide frames  */
    JDEC jd;
    JpgCtx ctx = { jpg, len, 0, fb[fb_front ^ 1], fbw, fbh };
    if (jd_prepare(&jd, jpg_in, work, sizeof(work), &ctx) != JDR_OK) return false;
    if (jd_decomp(&jd, jpg_out, 0) != JDR_OK) return false;      /* 1:1 */
    fb_front ^= 1;
    frame_rev_v++;
    return true;
}

/* ------------------------------------------------------------- stream task */
static void stream_task(void *)
{
    char url[96];
    snprintf(url, sizeof(url), "http://%s:%d/browse/stream?w=%d&h=%d",
             host, BROWSER_PORT, fbw, fbh);

    while (!stop_flag) {
        if (!host[0]) { snprintf(status_buf, sizeof(status_buf), "no companion host");
                        frame_rev_v++; vTaskDelay(pdMS_TO_TICKS(600)); continue; }
        HTTPClient http;
        http.setConnectTimeout(4000);
        http.setTimeout(30000);
        if (!http.begin(url)) { vTaskDelay(pdMS_TO_TICKS(600)); continue; }
        const int code = http.GET();
        if (code != 200) {
            snprintf(status_buf, sizeof(status_buf),
                     code == 503 ? "companion: pip install playwright" : "stream HTTP %d", code);
            http.end(); frame_rev_v++; vTaskDelay(pdMS_TO_TICKS(1200)); continue;
        }

        WiFiClient *s = http.getStreamPtr();
        const size_t CAP = 256 * 1024;
        uint8_t *acc = (uint8_t *)heap_caps_malloc(CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!acc) { http.end(); snprintf(status_buf, sizeof(status_buf), "no PSRAM"); break; }
        size_t   n = 0;
        uint32_t last = millis();
        streaming_v = true; status_buf[0] = '\0'; frame_rev_v++;

        while (!stop_flag) {
            const int avail = s->available();
            if (avail > 0) {
                const int r = s->read(acc + n, min((size_t)avail, CAP - n));
                if (r > 0) { n += r; last = millis(); }
            } else {
                if (!s->connected() || millis() - last > 15000) break;
                vTaskDelay(pdMS_TO_TICKS(3));
                continue;
            }
            size_t soi = SIZE_MAX, eoi = SIZE_MAX;
            for (size_t i = n; i >= 2; i--)
                if (acc[i-2] == 0xFF && acc[i-1] == 0xD9) { eoi = i; break; }
            if (eoi != SIZE_MAX)
                for (size_t i = eoi - 2; i >= 2; i--) {
                    if (acc[i-2] == 0xFF && acc[i-1] == 0xD8) { soi = i - 2; break; }
                    if (i == 2) break;
                }
            if (soi != SIZE_MAX && eoi != SIZE_MAX && eoi > soi) {
                decode_frame(acc + soi, eoi - soi);
                memmove(acc, acc + eoi, n - eoi);
                n -= eoi;
            } else if (n >= CAP - 8192) {
                n = 0;                          /* overflow without a frame: resync */
            }
        }
        heap_caps_free(acc);
        http.end();
        streaming_v = false;
    }
    stream_h = nullptr;
    vTaskDelete(nullptr);
}

/* -------------------------------------------------------------- input task */
static void input_task(void *)
{
    BEvent ev;
    while (!stop_flag) {
        if (xQueueReceive(input_q, &ev, pdMS_TO_TICKS(200)) != pdTRUE) continue;
        if (!host[0]) continue;
        char url[80];
        snprintf(url, sizeof(url), "http://%s:%d%s", host, BROWSER_PORT, ev.path);
        HTTPClient http;
        http.setConnectTimeout(3000);
        http.setTimeout(6000);
        if (http.begin(url)) {
            http.addHeader("Content-Type", "application/json");
            http.POST((uint8_t *)ev.body, strlen(ev.body));
            http.end();
        }
    }
    input_h = nullptr;
    vTaskDelete(nullptr);
}

/* ------------------------------------------------------------- enqueue API */
static void esc(char *dst, size_t cap, const char *src)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < cap; i++) {
        const char c = src[i];
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = c; }
        else if ((unsigned char)c >= 0x20)  dst[j++] = c;
    }
    dst[j] = '\0';
}

static void enqueue(const char *path, const char *body)
{
    if (!input_q) return;
    BEvent ev;
    snprintf(ev.path, sizeof(ev.path), "%s", path);
    snprintf(ev.body, sizeof(ev.body), "%s", body);
    xQueueSend(input_q, &ev, 0);
}

void browser_nav(const char *url)
{
    char e[150], b[192];
    esc(e, sizeof(e), url);
    snprintf(b, sizeof(b), "{\"type\":\"nav\",\"url\":\"%s\"}", e);
    enqueue("/browse/input", b);
}
void browser_input_tap(int x, int y)
{
    char b[64]; snprintf(b, sizeof(b), "{\"type\":\"tap\",\"x\":%d,\"y\":%d}", x, y);
    enqueue("/browse/input", b);
}
void browser_input_scroll(int dx, int dy)
{
    char b[72]; snprintf(b, sizeof(b), "{\"type\":\"scroll\",\"dx\":%d,\"dy\":%d}", dx, dy);
    enqueue("/browse/input", b);
}
void browser_input_text(const char *utf8)
{
    char e[150], b[192];
    esc(e, sizeof(e), utf8);
    snprintf(b, sizeof(b), "{\"type\":\"key\",\"text\":\"%s\"}", e);
    enqueue("/browse/input", b);
}
void browser_input_key(const char *key)
{
    char b[64]; snprintf(b, sizeof(b), "{\"type\":\"key\",\"key\":\"%s\"}", key);
    enqueue("/browse/input", b);
}
void browser_back(void)    { enqueue("/browse/input", "{\"type\":\"back\"}"); }
void browser_forward(void) { enqueue("/browse/input", "{\"type\":\"forward\"}"); }
void browser_reload(void)  { enqueue("/browse/input", "{\"type\":\"reload\"}"); }

/* ---------------------------------------------------------------- frame API */
const uint16_t *browser_frame(void)     { return fb[fb_front]; }
uint32_t        browser_frame_rev(void) { return frame_rev_v; }
int             browser_fb_w(void)      { return fbw; }
int             browser_fb_h(void)      { return fbh; }
bool            browser_streaming(void) { return streaming_v; }
const char     *browser_status(void)    { return status_buf; }

/* --------------------------------------------------------------- lifecycle */
bool browser_start(int w, int h)
{
    if (stream_h || input_h) return false;             /* previous still winding down */
    fbw = w; fbh = h;
    fb[0] = (uint16_t *)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    fb[1] = (uint16_t *)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!fb[0] || !fb[1]) {
        if (fb[0]) heap_caps_free(fb[0]);
        if (fb[1]) heap_caps_free(fb[1]);
        fb[0] = fb[1] = nullptr;
        snprintf(status_buf, sizeof(status_buf), "no PSRAM for page");
        return false;
    }
    memset(fb[0], 0, w * h * 2);
    memset(fb[1], 0, w * h * 2);
    fb_front = 0; frame_rev_v = 0;
    if (!input_q) input_q = xQueueCreate(16, sizeof(BEvent));
    else          xQueueReset(input_q);
    stop_flag = false; streaming_v = false;
    snprintf(status_buf, sizeof(status_buf), "connecting...");
    xTaskCreate(stream_task, "brws", 10240, nullptr, 4, &stream_h);
    xTaskCreate(input_task,  "brin", 8192,  nullptr, 4, &input_h);
    return true;
}

void browser_stop(void)
{
    stop_flag = true;
    /* join both tasks so nothing decodes into fb after we free it */
    for (int i = 0; i < 60 && (stream_h || input_h); i++) vTaskDelay(pdMS_TO_TICKS(15));
    streaming_v = false;
    if (fb[0]) { heap_caps_free(fb[0]); fb[0] = nullptr; }
    if (fb[1]) { heap_caps_free(fb[1]); fb[1] = nullptr; }
}

void browser_begin(void)
{
    bprefs.begin("browser", false);
    bprefs.getString("host", host, sizeof(host));
    if (!host[0]) {
        const char *h = yt_play_host();               /* reuse the YouTube companion IP */
        if (h && h[0]) snprintf(host, sizeof(host), "%s", h);
    }
}

void browser_set_host(const char *ip)
{
    snprintf(host, sizeof(host), "%s", ip);
    bprefs.putString("host", host);
}

const char *browser_host(void) { return host; }
