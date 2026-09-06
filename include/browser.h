/**
 * browser.h — the remote-browser worker (D057).
 *
 * The clock is a thin client: the Mac companion renders the page in headless
 * Chrome and streams JPEG frames (the YouTube player's wire format, D053);
 * this worker decodes the latest frame into a full-screen RGB565 buffer, and
 * POSTs taps / scrolls / keys back for Chrome to replay. Never touches LVGL —
 * the app blits browser_frame() from the LVGL task, as always.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void        browser_begin(void);
void        browser_set_host(const char *ip);   /* companion IP, :8999 */
const char *browser_host(void);

bool        browser_start(int w, int h);         /* alloc buffers, start tasks */
void        browser_stop(void);                  /* stop tasks, free buffers   */
bool        browser_streaming(void);
const char *browser_status(void);

void        browser_nav(const char *url);        /* URL or search text */
void        browser_input_tap(int x, int y);
void        browser_input_scroll(int dx, int dy);
void        browser_input_text(const char *utf8);/* type into focused field */
void        browser_input_key(const char *key);  /* "Backspace","Enter",...  */
void        browser_back(void);
void        browser_forward(void);
void        browser_reload(void);

const uint16_t *browser_frame(void);             /* front buffer, pre-swapped */
uint32_t        browser_frame_rev(void);
int             browser_fb_w(void);
int             browser_fb_h(void);
