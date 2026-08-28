/**
 * app_youtube.cpp — the YouTube dashboard (Tier A, D050).
 *
 * Latest uploads from the owner's channels, newest first, with thumbnails;
 * tapping a row throws the video to the Mac companion (tools/ytserve).
 * Setup lives inside the app: API key, channels, companion IP — each a
 * keyboard overlay, stored by yt.cpp in NVS, never in a file (D016 spirit).
 */
#include "app_api.h"
#include "app_host.h"
#include "yt.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define COL_BG    lv_color_hex(0x000000)
#define COL_CARD  lv_color_hex(0x161616)
#define COL_TEXT  lv_color_hex(0xE8E8E8)
#define COL_DIM   lv_color_hex(0x8A8A8A)
#define COL_ERR   lv_color_hex(0xE0483B)
#define COL_YT    lv_color_hex(0xE62117)

static lv_obj_t *scr, *list, *lbl_status, *setup_view, *overlay, *ov_ta;
static lv_obj_t *btn_refresh_lbl;
static uint32_t  seen_rev;
static int       ov_field;               /* 0 key, 1 channels, 2 host */
static lv_img_dsc_t thumb_dsc[YT_VIDEOS_N];

static void list_build(void);

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

/* ------------------------------------------------------- keyboard overlay */

static void overlay_close(void) { if (overlay) { lv_obj_del(overlay); overlay = nullptr; ov_ta = nullptr; } }

static void overlay_done(void)
{
    const char *txt = ov_ta ? lv_textarea_get_text(ov_ta) : "";
    if      (ov_field == 0) yt_set_key(txt);
    else if (ov_field == 1) yt_set_channels(txt);
    else                    yt_set_play_host(txt);
    overlay_close();
    if (setup_view) { lv_obj_del(setup_view); setup_view = nullptr; }
    yt_request_refresh();
    lv_label_set_text(lbl_status, "saved - refreshing");
}

static void ov_kb_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY)  overlay_done();
    if (c == LV_EVENT_CANCEL) overlay_close();
}

static void overlay_open(int field)
{
    if (overlay) return;
    ov_field = field;
    static const char *cap[3] = {
        "API key (Google Cloud console, YouTube Data API v3)",
        "Channels: @handle, comma separated (up to 6)",
        "Companion IP (the Mac running tools/ytserve)" };
    static const char *ph[3] = { "AIza...", "@mkbhd, @veritasium", "192.168.0.10" };

    overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *c = lv_label_create(overlay);
    lv_obj_set_style_text_font(c, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(c, COL_DIM, LV_PART_MAIN);
    lv_obj_set_width(c, LV_PCT(94));
    lv_label_set_long_mode(c, LV_LABEL_LONG_WRAP);
    lv_label_set_text(c, cap[field]);
    lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 8);

    ov_ta = lv_textarea_create(overlay);
    lv_obj_set_size(ov_ta, LV_PCT(94), 56);
    lv_obj_align(ov_ta, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_text_font(ov_ta, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_textarea_set_one_line(ov_ta, true);
    lv_textarea_set_placeholder_text(ov_ta, ph[field]);
    if (field == 1) {
        char cur[200] = "";
        for (int i = 0; i < yt_channel_count(); i++) {
            strlcat(cur, yt_channel(i), sizeof(cur));
            if (i + 1 < yt_channel_count()) strlcat(cur, ",", sizeof(cur));
        }
        lv_textarea_set_text(ov_ta, cur);
    } else if (field == 2) {
        lv_textarea_set_text(ov_ta, yt_play_host());
    }

    lv_obj_t *kb = lv_keyboard_create(overlay);
    lv_obj_set_size(kb, LV_PCT(100), lv_disp_get_ver_res(nullptr) > 500 ? 280 : 240);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ov_ta);
    lv_obj_add_event_cb(kb, ov_kb_cb, LV_EVENT_ALL, nullptr);
}

/* ---------------------------------------------------------------- setup -- */

static void setup_row_cb(lv_event_t *e) { overlay_open((int)(intptr_t)lv_event_get_user_data(e)); }

static void setup_open(lv_event_t *)
{
    if (setup_view) return;
    setup_view = lv_obj_create(scr);
    lv_obj_remove_style_all(setup_view);
    lv_obj_set_size(setup_view, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(setup_view, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(setup_view, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(setup_view, LV_OBJ_FLAG_SCROLLABLE);

    static const char *names[3] = { "API key", "Channels", "Companion IP" };
    char vals[3][64];
    snprintf(vals[0], sizeof(vals[0]), "%s", yt_has_key() ? "set" : "not set");
    snprintf(vals[1], sizeof(vals[1]), "%d configured", yt_channel_count());
    snprintf(vals[2], sizeof(vals[2]), "%s", yt_play_host()[0] ? yt_play_host() : "not set");

    for (int i = 0; i < 3; i++) {
        lv_obj_t *row = lv_obj_create(setup_view);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(94), 56);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 16 + i * 66);
        lv_obj_set_style_bg_color(row, COL_CARD, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, 14, LV_PART_MAIN);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, setup_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *n = lv_label_create(row);
        lv_obj_set_style_text_font(n, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(n, COL_TEXT, LV_PART_MAIN);
        lv_label_set_text(n, names[i]);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *v = lv_label_create(row);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(v, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(v, vals[i]);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_obj_t *hint = lv_label_create(setup_view);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, COL_DIM, LV_PART_MAIN);
    lv_obj_set_width(hint, LV_PCT(94));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_label_set_text(hint, "Everything can also be set from a shell:\n"
                            "POST /youtube {\"key\":...} {\"channels\":\"@a,@b\"} {\"host\":...}");
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 16 + 3 * 66 + 8);
}

/* ----------------------------------------------------------------- list -- */

static void row_cb(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    const YtVideo *v = yt_video(i);
    if (!v) return;
    yt_request_play(v->id);
    lv_label_set_text(lbl_status, "sending to the Mac...");
}

static void list_build(void)
{
    if (!list) return;
    lv_obj_clean(list);
    seen_rev = yt_rev();

    if (!yt_has_key() || yt_channel_count() == 0) {
        lv_obj_t *l = lv_label_create(list);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, COL_DIM, LV_PART_MAIN);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_label_set_text(l, yt_has_key()
            ? "No channels yet. Setup > Channels, or POST /youtube."
            : "No API key yet.\n\nGoogle Cloud console > YouTube Data API v3 >"
              " credentials > API key, then Setup > API key here, or\n"
              "curl -X POST flipclock.local/youtube -d '{\"key\":\"AIza...\"}'");
        return;
    }

    if (yt_video_count() == 0) {
        lv_obj_t *l = lv_label_create(list);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(l, yt_busy() ? "fetching..." : "nothing yet - tap Refresh");
        return;
    }

    const int W = lv_disp_get_hor_res(nullptr);
    for (int i = 0; i < yt_video_count(); i++) {
        const YtVideo *v = yt_video(i);
        if (!v) break;

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 102);
        lv_obj_set_style_bg_color(row, COL_CARD, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 6, LV_PART_MAIN);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        if (v->thumb) {
            thumb_dsc[i].header.always_zero = 0;
            thumb_dsc[i].header.w  = YT_THUMB_W;
            thumb_dsc[i].header.h  = YT_THUMB_H;
            thumb_dsc[i].header.cf = LV_IMG_CF_TRUE_COLOR;
            thumb_dsc[i].data_size = YT_THUMB_W * YT_THUMB_H * 2;
            thumb_dsc[i].data      = (const uint8_t *)v->thumb;
            lv_obj_t *img = lv_img_create(row);
            lv_img_set_src(img, &thumb_dsc[i]);
            lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
        } else {
            lv_obj_t *ph = lv_obj_create(row);
            decor(ph);
            lv_obj_set_size(ph, YT_THUMB_W, YT_THUMB_H);
            lv_obj_set_style_bg_color(ph, lv_color_hex(0x0A0A0A), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(ph, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(ph, 8, LV_PART_MAIN);
            lv_obj_align(ph, LV_ALIGN_LEFT_MID, 0, 0);
        }

        lv_obj_t *t = lv_label_create(row);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(t, COL_TEXT, LV_PART_MAIN);
        lv_obj_set_width(t, W * (100 - 4) / 100 - YT_THUMB_W - 26);
        lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
        lv_obj_set_height(t, 44);
        lv_label_set_text(t, v->title);
        lv_obj_set_pos(t, YT_THUMB_W + 12, 6);

        char sub[48];
        snprintf(sub, sizeof(sub), "%s   ·   %s ago", v->channel, v->age);
        lv_obj_t *s = lv_label_create(row);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(s, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(s, sub);
        lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, YT_THUMB_W + 12, -8);
    }
}

/* ------------------------------------------------------------------ app -- */

static void refresh_cb(lv_event_t *)
{
    if (yt_busy()) return;
    yt_request_refresh();
    lv_label_set_text(btn_refresh_lbl, "...");
    lv_label_set_text(lbl_status, "fetching");
}

static void yt_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = lv_color_hex(0xE62117);
    r.bg_opa   = LV_OPA_COVER;
    r.radius   = 14;
    lv_area_t a = { (lv_coord_t)(cx - 36), (lv_coord_t)(cy - 26),
                    (lv_coord_t)(cx + 36), (lv_coord_t)(cy + 26) };
    lv_draw_rect(ctx, &r, &a);

    lv_draw_rect_dsc_t w;
    lv_draw_rect_dsc_init(&w);
    w.bg_color = lv_color_hex(0xFFFFFF);
    w.bg_opa   = LV_OPA_COVER;
    /* the play triangle, as three shrinking bars — no polygon API needed */
    for (int i = 0; i < 12; i++) {
        lv_area_t bar = { (lv_coord_t)(cx - 10 + i), (lv_coord_t)(cy - 14 + i),
                          (lv_coord_t)(cx - 9 + i),  (lv_coord_t)(cy + 14 - i) };
        lv_draw_rect(ctx, &w, &bar);
    }
}

static lv_obj_t *yt_create(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);
    const bool portrait = H > W;

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(t, "YouTube   ·   tap a video to play it on the Mac");
    lv_obj_set_pos(t, 14, 10);

    list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LV_PCT(96), H - 40 - 76 - 8);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);

    lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_status, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(lbl_status, "");
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_LEFT, 156, -74);

    app_host_std_back(scr, nullptr);

    const int bw = portrait ? 135 : 200;
    lv_obj_t *rf = lv_btn_create(scr);
    lv_obj_set_size(rf, bw, 56);
    lv_obj_align(rf, LV_ALIGN_BOTTOM_RIGHT, -12 - bw - 8, -10);
    lv_obj_add_event_cb(rf, refresh_cb, LV_EVENT_CLICKED, nullptr);
    btn_refresh_lbl = lv_label_create(rf);
    lv_obj_set_style_text_font(btn_refresh_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(btn_refresh_lbl, LV_SYMBOL_REFRESH "  Refresh");
    lv_obj_center(btn_refresh_lbl);

    lv_obj_t *st = lv_btn_create(scr);
    lv_obj_set_size(st, bw, 56);
    lv_obj_align(st, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_set_style_bg_color(st, COL_YT, LV_PART_MAIN);
    lv_obj_add_event_cb(st, setup_open, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *sl = lv_label_create(st);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(sl, LV_SYMBOL_SETTINGS "  Setup");
    lv_obj_center(sl);

    seen_rev = yt_rev() - 1;
    list_build();
    if (yt_has_key() && yt_channel_count() > 0 && yt_video_count() == 0)
        yt_request_refresh();
    return scr;
}

static void yt_destroy(void)
{
    scr = list = lbl_status = setup_view = overlay = ov_ta = nullptr;
    btn_refresh_lbl = nullptr;
}

static void yt_tick(void)
{
    if (!scr) return;
    if (yt_rev() != seen_rev) {
        seen_rev = yt_rev();
        if (!setup_view && !overlay) list_build();
        const char *st = yt_status();
        lv_label_set_text(lbl_status, st);
        lv_obj_set_style_text_color(lbl_status,
            (strstr(st, "HTTP") || strstr(st, "no ") || strstr(st, "offline"))
                ? COL_ERR : COL_DIM, LV_PART_MAIN);
    }
    if (!yt_busy() && btn_refresh_lbl &&
        strcmp(lv_label_get_text(btn_refresh_lbl), "...") == 0)
        lv_label_set_text(btn_refresh_lbl, LV_SYMBOL_REFRESH "  Refresh");
}

static bool yt_back(void)
{
    if (overlay)    { overlay_close(); return true; }
    if (setup_view) { lv_obj_del(setup_view); setup_view = nullptr; list_build(); return true; }
    return false;
}

extern const App app_youtube = { "YouTube", yt_icon, yt_create, yt_destroy,
                                 yt_tick, yt_back, /*portrait_ok=*/true };
