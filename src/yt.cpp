#include "yt.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
/*
 * LVGL's bundled TJpgDec, NOT the ROM's — and that word "not" cost a crash
 * loop (v1.30.2). LV_USE_SJPG=1 compiles lvgl's tjpgd.c with the same
 * jd_prepare/jd_decomp symbol names, and at link time a real object file
 * beats the ROM's linker alias. v1.30.0 included the ROM header, so the
 * structs were laid out per the ROM while the CODE that ran was LVGL's:
 * jd_prepare scribbled by its own layout, jpg_in read a device pointer made
 * of 0xA5 stack fill, LoadProhibited, reboot — on the first thumbnail of
 * the first refresh, every time the app opened. The header must match the
 * implementation that LINKS, and this one provably does.
 */
extern "C" {
#include "src/extra/libs/sjpg/tjpgd.h"
}

static Preferences prefs;
static char key_buf[48];
static char channels_buf[YT_CHANNELS_N][32];
static int  channels_n;
static char play_host[20];

static YtVideo  videos[YT_VIDEOS_N];
static int      video_n;
static volatile uint32_t rev_v;
static volatile bool     busy;
static char     status_buf[64];
static uint8_t  mode_v;                  /* 0 channels, 1 popular, 2 search */
static char     search_q[64];
static char     region[4];

enum CmdType { CMD_REFRESH = 1, CMD_PLAY, CMD_STREAM };
static struct { CmdType type; char id[16]; } cmd;
static QueueHandle_t cmd_q;

/* ------------------------------------------------------------------ config */

bool yt_has_key(void)          { return key_buf[0] != '\0'; }
int  yt_channel_count(void)    { return channels_n; }
const char *yt_channel(int i)  { return (i >= 0 && i < channels_n) ? channels_buf[i] : ""; }
const char *yt_play_host(void) { return play_host; }
uint32_t yt_rev(void)          { return rev_v; }
int  yt_video_count(void)      { return video_n; }
const YtVideo *yt_video(int i) { return (i >= 0 && i < video_n) ? &videos[i] : nullptr; }
const char *yt_status(void)    { return status_buf; }
bool yt_busy(void)             { return busy; }

void yt_set_key(const char *k)
{
    snprintf(key_buf, sizeof(key_buf), "%s", k ? k : "");
    prefs.putString("key", key_buf);
}

void yt_set_channels(const char *csv)
{
    channels_n = 0;
    if (csv) {
        char tmp[200];
        snprintf(tmp, sizeof(tmp), "%s", csv);
        for (char *t = strtok(tmp, ", "); t && channels_n < YT_CHANNELS_N;
             t = strtok(nullptr, ", ")) {
            snprintf(channels_buf[channels_n], sizeof(channels_buf[0]), "%s", t);
            channels_n++;
        }
    }
    char joined[200] = "";
    for (int i = 0; i < channels_n; i++) {
        strlcat(joined, channels_buf[i], sizeof(joined));
        if (i + 1 < channels_n) strlcat(joined, ",", sizeof(joined));
    }
    prefs.putString("chans", joined);
    /* A changed channel list may orphan the cached playlist ids; drop them. */
    prefs.remove("plmap");
}

/* ------------------------------------------------------- streaming state */
static uint16_t *fb[2];
static volatile uint8_t  fb_front;
static volatile uint32_t frame_rev_v;
static volatile bool     stream_stop_flag, streaming_v;
static char     stream_id[16];

bool     yt_streaming(void) { return streaming_v; }
uint32_t yt_frame_rev(void) { return frame_rev_v; }
const uint16_t *yt_frame(void) { return fb[fb_front]; }

void yt_stream_stop(void) { stream_stop_flag = true; }

void yt_stream_start(const char *id)
{
    if (busy || !id || !id[0] || !play_host[0]) return;
    decltype(cmd) c = {}; c.type = CMD_STREAM;
    snprintf(c.id, sizeof(c.id), "%s", id);
    stream_stop_flag = false;
    xQueueSend(cmd_q, &c, 0);
}

int  yt_mode(void) { return mode_v; }
void yt_set_mode(int m)
{
    if (m < 0 || m > 2) return;
    mode_v = (uint8_t)m;
}

void yt_set_region(const char *r)
{
    snprintf(region, sizeof(region), "%s", (r && r[0]) ? r : "AE");
    prefs.putString("region", region);
}

void yt_set_search(const char *q)
{
    snprintf(search_q, sizeof(search_q), "%s", q ? q : "");
    mode_v = 2;
}
const char *yt_search_query(void) { return search_q; }

void yt_set_play_host(const char *ip)
{
    snprintf(play_host, sizeof(play_host), "%s", ip ? ip : "");
    prefs.putString("host", play_host);
}

/* ------------------------------------------------------------------- https */

/*
 * TLS without certificate pinning, stated plainly: this fetches PUBLIC
 * read-only data (titles and thumbnails) on a desk gadget; the bundle of CA
 * roots does not fit the maintenance budget, and the failure a forged
 * googleapis would cause here is a wrong thumbnail. The API KEY rides the
 * query string either way, as Google's docs specify.
 */
static bool https_get(const char *url, String &out, size_t cap)
{
    WiFiClientSecure ssl;
    ssl.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(8000);
    if (!http.begin(ssl, url)) { snprintf(status_buf, sizeof(status_buf), "begin failed"); return false; }
    const int code = http.GET();
    if (code != 200) {
        snprintf(status_buf, sizeof(status_buf), "HTTP %d%s", code,
                 code == 403 ? " (quota or bad key?)" : "");
        http.end();
        return false;
    }
    out = http.getString();
    http.end();
    if (out.length() == 0 || out.length() > cap) {
        snprintf(status_buf, sizeof(status_buf), "response %u bytes", out.length());
        return false;
    }
    return true;
}

static bool https_get_bin(const char *url, uint8_t **buf, size_t *len)
{
    WiFiClientSecure ssl;
    ssl.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(8000);
    if (!http.begin(ssl, url)) return false;
    if (http.GET() != 200) { http.end(); return false; }
    const int n = http.getSize();
    if (n <= 0 || n > 80 * 1024) { http.end(); return false; }
    *buf = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!*buf) { http.end(); return false; }
    WiFiClient *s = http.getStreamPtr();
    size_t got = 0;
    uint32_t t0 = millis();
    while (got < (size_t)n && millis() - t0 < 8000) {
        int r = s->read(*buf + got, n - got);
        if (r > 0) got += r;
        else delay(5);
    }
    http.end();
    if (got != (size_t)n) { heap_caps_free(*buf); *buf = nullptr; return false; }
    *len = got;
    return true;
}

/* ------------------------------------------------- ROM TJpgDec -> RGB565 -- */

struct JpgCtx { const uint8_t *data; size_t len, pos; uint16_t *out; int ow, oh; };

static size_t jpg_in(JDEC *jd, uint8_t *buf, size_t n)
{
    JpgCtx *c = (JpgCtx *)jd->device;
    if (c->pos + n > c->len) n = c->len - c->pos;
    if (buf) memcpy(buf, c->data + c->pos, n);
    c->pos += n;
    return n;
}

/* LVGL's build is JD_FORMAT=0: RGB888 in, we pack to 565. */
static int jpg_out(JDEC *jd, void *bitmap, JRECT *r)
{
    JpgCtx *c = (JpgCtx *)jd->device;
    const uint8_t *p = (const uint8_t *)bitmap;
    for (int y = r->top; y <= r->bottom; y++) {
        for (int x = r->left; x <= r->right; x++) {
            if (x < c->ow && y < c->oh) {
                uint16_t px = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3);
#if LV_COLOR_16_SWAP
                /* The panel takes big-endian 565 and LVGL stores TRUE_COLOR
                 * buffers pre-swapped when LV_COLOR_16_SWAP=1 — without this
                 * every thumbnail's bytes are crossed and the colors smear
                 * (v1.31.0, owner-reported). */
                px = (uint16_t)((px >> 8) | (px << 8));
#endif
                c->out[y * c->ow + x] = px;
            }
            p += 3;
        }
    }
    return 1;
}

static uint16_t *decode_thumb(const uint8_t *jpg, size_t len)
{
    static uint8_t work[3800];      /* TJpgDec workspace, worker task only */
    JDEC jd;
    JpgCtx ctx = { jpg, len, 0, nullptr, YT_THUMB_W, YT_THUMB_H };
    if (jd_prepare(&jd, jpg_in, work, sizeof(work), &ctx) != JDR_OK) return nullptr;
    ctx.out = (uint16_t *)heap_caps_malloc(YT_THUMB_W * YT_THUMB_H * 2,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ctx.out) return nullptr;
    /* mqdefault is 320x180; scale 1 = 1/2 lands exactly on 160x90 */
    if (jd_decomp(&jd, jpg_out, 1) != JDR_OK) { heap_caps_free(ctx.out); return nullptr; }
    return ctx.out;
}

/* ---------------------------------------------------------------- stream -- */
/*
 * The player loop (D053). Plain HTTP from the companion, a rolling buffer,
 * SOI/EOI framing, decode-latest-drop-stale — the natural frame-skip when
 * decode (~35 ms) falls behind the 12 fps the Mac sends. Runs entirely on
 * this worker; the LVGL task only ever blits the front buffer.
 */
static bool decode_frame(const uint8_t *jpg, size_t len)
{
    static uint8_t work[3800];
    JDEC jd;
    JpgCtx ctx = { jpg, len, 0, fb[fb_front ^ 1], YT_FRAME_W, YT_FRAME_H };
    if (jd_prepare(&jd, jpg_in, work, sizeof(work), &ctx) != JDR_OK) return false;
    if (jd_decomp(&jd, jpg_out, 0) != JDR_OK) return false;   /* 1:1 */
    fb_front ^= 1;
    frame_rev_v++;
    return true;
}

static void do_stream(const char *id)
{
    if (!fb[0]) {
        fb[0] = (uint16_t *)heap_caps_malloc(YT_FRAME_W * YT_FRAME_H * 2,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        fb[1] = (uint16_t *)heap_caps_malloc(YT_FRAME_W * YT_FRAME_H * 2,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!fb[0] || !fb[1]) { snprintf(status_buf, sizeof(status_buf), "no PSRAM for frames"); return; }
    }

    char url[80];
    snprintf(url, sizeof(url), "http://%s:8999/stream/%s", play_host, id);
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(30000);            /* yt-dlp resolve can take a while  */
    if (!http.begin(url)) return;
    const int code = http.GET();
    if (code != 200) {
        snprintf(status_buf, sizeof(status_buf),
                 code == 503 ? "companion: install yt-dlp" : "stream HTTP %d", code);
        http.end();
        rev_v++;
        return;
    }

    WiFiClient *s = http.getStreamPtr();
    const size_t CAP = 96 * 1024;
    uint8_t *acc = (uint8_t *)heap_caps_malloc(CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t   n = 0;
    uint32_t last_data = millis();
    streaming_v = true;
    snprintf(status_buf, sizeof(status_buf), "playing here");
    rev_v++;

    while (!stream_stop_flag) {
        int avail = s->available();
        if (avail > 0) {
            int r = s->read(acc + n, min((size_t)avail, CAP - n));
            if (r > 0) { n += r; last_data = millis(); }
        } else {
            if (!s->connected() || millis() - last_data > 15000) break;
            delay(5);
            continue;
        }

        /* Latest complete SOI..EOI wins; everything before it is stale. */
        size_t soi = SIZE_MAX, eoi = SIZE_MAX;
        for (size_t i = n; i >= 2; i--) {
            if (acc[i-2] == 0xFF && acc[i-1] == 0xD9) { eoi = i; break; }
        }
        if (eoi != SIZE_MAX) {
            for (size_t i = eoi - 2; i >= 2; i--) {
                if (acc[i-2] == 0xFF && acc[i-1] == 0xD8) { soi = i - 2; break; }
                if (i == 2) break;
            }
        }
        if (soi != SIZE_MAX && eoi != SIZE_MAX && eoi > soi) {
            decode_frame(acc + soi, eoi - soi);
            memmove(acc, acc + eoi, n - eoi);
            n -= eoi;
        } else if (n >= CAP - 4096) {
            n = 0;                       /* garbage overflow: resync */
        }
    }

    heap_caps_free(acc);
    http.end();
    streaming_v = false;
    snprintf(status_buf, sizeof(status_buf), stream_stop_flag ? "" : "stream ended");
    rev_v++;
}

/* ----------------------------------------------------------------- refresh */

static void age_from_iso(const char *iso, char *out, size_t cap)
{
    /* "2026-08-28T09:15:00Z" — coarse age from the parts we can trust. */
    struct tm tmv = {};
    if (!iso || strlen(iso) < 19 || !strptime(iso, "%Y-%m-%dT%H:%M:%S", &tmv)) {
        snprintf(out, cap, "");
        return;
    }
    time_t then = mktime(&tmv);      /* both sides UTC-ish; coarse is fine */
    time_t now  = time(nullptr);
    long s = (long)difftime(now, then);
    if (s < 0) s = 0;
    if      (s < 3600)        snprintf(out, cap, "%ldm", s / 60);
    else if (s < 86400)       snprintf(out, cap, "%ldh", s / 3600);
    else                      snprintf(out, cap, "%ldd", s / 86400);
}

static void videos_clear(void)
{
    for (int i = 0; i < YT_VIDEOS_N; i++) {
        if (videos[i].thumb) { heap_caps_free(videos[i].thumb); videos[i].thumb = nullptr; }
    }
    video_n = 0;
}

static bool uploads_playlist_for(const char *handle, char *out, size_t cap,
                                 char *title, size_t tcap)
{
    /* Cached from an earlier resolve? plmap holds "handle=PL,handle=PL". */
    String map = prefs.getString("plmap", "");
    char want[40];
    snprintf(want, sizeof(want), "%s=", handle);
    int at = map.indexOf(want);
    if (at >= 0) {
        int end = map.indexOf(',', at);
        String pl = map.substring(at + strlen(want), end < 0 ? map.length() : end);
        int bar = pl.indexOf('|');
        if (bar > 0) {
            snprintf(out, cap, "%s", pl.substring(0, bar).c_str());
            snprintf(title, tcap, "%s", pl.substring(bar + 1).c_str());
            return true;
        }
    }

    char url[256];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/youtube/v3/channels"
             "?part=contentDetails,snippet&forHandle=%s"
             "&fields=items(snippet/title,contentDetails/relatedPlaylists/uploads)"
             "&key=%s", handle, key_buf);
    String body;
    if (!https_get(url, body, 8192)) return false;

    DynamicJsonDocument doc(3072);
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        snprintf(status_buf, sizeof(status_buf), "channel parse failed");
        return false;
    }
    JsonVariant item = doc["items"][0];
    if (item.isNull()) {
        snprintf(status_buf, sizeof(status_buf), "no channel: %s", handle);
        return false;
    }
    snprintf(out, cap, "%s", (const char *)item["contentDetails"]["relatedPlaylists"]["uploads"]);
    snprintf(title, tcap, "%s", (const char *)item["snippet"]["title"]);

    map += want; map += out; map += "|"; map += title; map += ",";
    prefs.putString("plmap", map);
    return true;
}

struct Fetched { char id[16]; char title[YT_TITLE_MAX + 1]; char chan[28];
                 char iso[24]; char thumb_url[96]; };

/* Minimal query escaper: spaces and the reserved handful. */
static void url_escape(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    for (const char *p = in; *p && o + 4 < cap; p++) {
        if (isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.')
            out[o++] = *p;
        else
            o += snprintf(out + o, cap - o, "%%%02X", (unsigned char)*p);
    }
    out[o] = '\0';
}

/* One JSON video list -> Fetched[]; search and popular share the shape,
 * they just keep the id in different places. */
static int parse_items(const String &body, Fetched *all, int cap, bool id_nested)
{
    DynamicJsonDocument doc(12288);
    if (deserializeJson(doc, body) != DeserializationError::Ok) return 0;
    int n = 0;
    for (JsonVariant v : doc["items"].as<JsonArray>()) {
        if (n >= cap) break;
        Fetched &f = all[n];
        const char *id = id_nested ? (const char *)v["id"]["videoId"]
                                   : (const char *)v["id"];
        snprintf(f.id,    sizeof(f.id),    "%s", id ? id : "");
        snprintf(f.title, sizeof(f.title), "%s", (const char *)v["snippet"]["title"]);
        snprintf(f.chan,  sizeof(f.chan),  "%s", (const char *)v["snippet"]["channelTitle"]);
        snprintf(f.iso,   sizeof(f.iso),   "%s", (const char *)v["snippet"]["publishedAt"]);
        snprintf(f.thumb_url, sizeof(f.thumb_url), "%s",
                 (const char *)v["snippet"]["thumbnails"]["medium"]["url"]);
        if (f.id[0]) n++;
    }
    return n;
}

static void do_refresh(void)
{
    status_buf[0] = '\0';
    if (!yt_has_key())    { snprintf(status_buf, sizeof(status_buf), "no API key"); return; }

    static Fetched all[YT_CHANNELS_N * 3];
    int all_n = 0;

    if (mode_v == 1) {
        /* Trending for the region — the closest thing to a Home feed the
         * key-only API allows (D050 addendum). One unit. */
        char url[300];
        snprintf(url, sizeof(url),
                 "https://www.googleapis.com/youtube/v3/videos"
                 "?part=snippet&chart=mostPopular&maxResults=%d&regionCode=%s"
                 "&fields=items(id,snippet(title,channelTitle,publishedAt,"
                 "thumbnails/medium/url))&key=%s",
                 YT_VIDEOS_N, region, key_buf);
        String body;
        if (https_get(url, body, 20000))
            all_n = parse_items(body, all, YT_VIDEOS_N, false);
    } else if (mode_v == 2) {
        if (!search_q[0]) { snprintf(status_buf, sizeof(status_buf), "empty search"); return; }
        char q[200];
        url_escape(search_q, q, sizeof(q));
        char url[400];
        /* 100 quota units per search — the expensive call, on demand only. */
        snprintf(url, sizeof(url),
                 "https://www.googleapis.com/youtube/v3/search"
                 "?part=snippet&type=video&maxResults=%d&q=%s"
                 "&fields=items(id/videoId,snippet(title,channelTitle,"
                 "publishedAt,thumbnails/medium/url))&key=%s",
                 YT_VIDEOS_N, q, key_buf);
        String body;
        if (https_get(url, body, 20000))
            all_n = parse_items(body, all, YT_VIDEOS_N, true);
    } else {
    if (channels_n == 0)  { snprintf(status_buf, sizeof(status_buf), "no channels configured"); return; }

    for (int c = 0; c < channels_n; c++) {
        char pl[40], ctitle[28];
        if (!uploads_playlist_for(channels_buf[c], pl, sizeof(pl),
                                  ctitle, sizeof(ctitle))) continue;

        char url[320];
        snprintf(url, sizeof(url),
                 "https://www.googleapis.com/youtube/v3/playlistItems"
                 "?part=snippet&maxResults=3&playlistId=%s"
                 "&fields=items(snippet(title,publishedAt,resourceId/videoId,"
                 "thumbnails/medium/url))&key=%s", pl, key_buf);
        String body;
        if (!https_get(url, body, 16384)) continue;

        DynamicJsonDocument doc(8192);
        if (deserializeJson(doc, body) != DeserializationError::Ok) continue;
        for (JsonVariant v : doc["items"].as<JsonArray>()) {
            if (all_n >= (int)(sizeof(all) / sizeof(all[0]))) break;
            Fetched &f = all[all_n];
            snprintf(f.id,    sizeof(f.id),    "%s", (const char *)v["snippet"]["resourceId"]["videoId"]);
            snprintf(f.title, sizeof(f.title), "%s", (const char *)v["snippet"]["title"]);
            snprintf(f.chan,  sizeof(f.chan),  "%s", ctitle);
            snprintf(f.iso,   sizeof(f.iso),   "%s", (const char *)v["snippet"]["publishedAt"]);
            snprintf(f.thumb_url, sizeof(f.thumb_url), "%s",
                     (const char *)v["snippet"]["thumbnails"]["medium"]["url"]);
            if (f.id[0]) all_n++;
        }
    }

    }   /* channels mode */

    if (all_n == 0 && !status_buf[0])
        snprintf(status_buf, sizeof(status_buf), "nothing fetched");

    /* Channels merge newest-first; popular and search keep API order. */
    if (mode_v == 0)
        for (int i = 0; i < all_n; i++)
            for (int j = i + 1; j < all_n; j++)
                if (strcmp(all[j].iso, all[i].iso) > 0) { Fetched t = all[i]; all[i] = all[j]; all[j] = t; }

    videos_clear();
    for (int i = 0; i < all_n && video_n < YT_VIDEOS_N; i++) {
        YtVideo &v = videos[video_n];
        snprintf(v.id,      sizeof(v.id),      "%s", all[i].id);
        snprintf(v.title,   sizeof(v.title),   "%s", all[i].title);
        snprintf(v.channel, sizeof(v.channel), "%s", all[i].chan);
        age_from_iso(all[i].iso, v.age, sizeof(v.age));
        v.thumb = nullptr;
        uint8_t *jpg; size_t jlen;
        if (all[i].thumb_url[0] && https_get_bin(all[i].thumb_url, &jpg, &jlen)) {
            v.thumb = decode_thumb(jpg, jlen);
            heap_caps_free(jpg);
        }
        video_n++;
        rev_v++;                     /* rows appear as they arrive */
    }
    Serial.printf("[yt] %d videos, status '%s'\n", video_n, status_buf);
    rev_v++;
}

static void do_play(const char *id)
{
    if (!play_host[0]) { snprintf(status_buf, sizeof(status_buf), "no companion host set"); rev_v++; return; }
    char url[64], body[64];
    snprintf(url, sizeof(url), "http://%s:8999/play", play_host);
    snprintf(body, sizeof(body), "{\"id\":\"%s\"}", id);
    HTTPClient http;
    http.setConnectTimeout(2500);
    http.setTimeout(4000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST((uint8_t *)body, strlen(body));
    http.end();
    snprintf(status_buf, sizeof(status_buf),
             code == 200 ? "playing on the Mac" : "companion offline (%d)", code);
    rev_v++;
}

static void yt_task(void *)
{
    for (;;) {
        if (xQueueReceive(cmd_q, &cmd, portMAX_DELAY) != pdTRUE) continue;
        busy = true;
        if      (cmd.type == CMD_REFRESH) do_refresh();
        else if (cmd.type == CMD_PLAY)    do_play(cmd.id);
        else if (cmd.type == CMD_STREAM)  do_stream(cmd.id);
        busy = false;
    }
}

void yt_request_refresh(void)
{
    if (busy) return;
    decltype(cmd) c = {}; c.type = CMD_REFRESH;
    xQueueSend(cmd_q, &c, 0);
}

void yt_request_play(const char *id)
{
    if (busy || !id || !id[0]) return;
    decltype(cmd) c = {}; c.type = CMD_PLAY;
    snprintf(c.id, sizeof(c.id), "%s", id);
    xQueueSend(cmd_q, &c, 0);
}

void yt_begin(void)
{
    prefs.begin("yt", false);
    prefs.getString("key",   key_buf,  sizeof(key_buf));
    prefs.getString("host",  play_host, sizeof(play_host));
    char chans[200] = "";
    prefs.getString("chans", chans, sizeof(chans));
    prefs.getString("region", region, sizeof(region));
    if (!region[0]) snprintf(region, sizeof(region), "AE");
    channels_n = 0;
    for (char *t = strtok(chans, ","); t && channels_n < YT_CHANNELS_N;
         t = strtok(nullptr, ",")) {
        snprintf(channels_buf[channels_n], sizeof(channels_buf[0]), "%s", t);
        channels_n++;
    }
    cmd_q = xQueueCreate(2, sizeof(cmd));
    xTaskCreatePinnedToCore(yt_task, "yt", 10240, nullptr, 1, nullptr, 0);
}
