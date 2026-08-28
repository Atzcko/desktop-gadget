/**
 * yt.h — the YouTube dashboard's data layer. See D050.
 *
 * Tier A of the YouTube assessment: the OFFICIAL Data API v3 with an API
 * key, showing the latest uploads of channels the owner picks, with
 * thumbnails — and tap-to-play THROWS the video to the companion on the Mac
 * (tools/ytserve), because this device can display a video's face but not
 * its body (no codec, no audio path).
 *
 * Everything network runs on a worker task; the UI requests and polls, the
 * same shape as messaging. The key is provisioned like Wi-Fi credentials:
 * never in a file, POSTed or typed once, kept in NVS (D016's doctrine).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define YT_CHANNELS_N   6
#define YT_VIDEOS_N     8
#define YT_TITLE_MAX    80
#define YT_THUMB_W      160
#define YT_THUMB_H      90

struct YtVideo {
    char      id[16];
    char      title[YT_TITLE_MAX + 1];
    char      channel[28];
    char      age[16];              /* "3h" / "2d", precomputed */
    uint16_t *thumb;                /* RGB565 160x90 in PSRAM, or null */
};

void yt_begin(void);

/* config, persisted in the module's own NVS namespace */
bool yt_has_key(void);
void yt_set_key(const char *key);
void yt_set_channels(const char *csv);      /* "@mkbhd,@veritasium" */
int  yt_channel_count(void);
const char *yt_channel(int i);
void yt_set_play_host(const char *ip);
const char *yt_play_host(void);

/* worker */
/* 0 = channel latest, 1 = popular (regional trending), 2 = search */
int  yt_mode(void);
void yt_set_mode(int m);
void yt_set_search(const char *q);         /* sets mode 2 */
const char *yt_search_query(void);
void yt_set_region(const char *r);         /* "AE", "US", ... */

void yt_request_refresh(void);
void yt_request_play(const char *video_id);
bool yt_busy(void);

/* results */
uint32_t yt_rev(void);
int      yt_video_count(void);
const YtVideo *yt_video(int i);
const char *yt_status(void);        /* "", or a short human error */
