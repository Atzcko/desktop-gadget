/**
 * app_messages.cpp — talk to the other gadgets. See D046.
 *
 * Peers arrive from an mDNS scan (worker task; Scan button), the inbox from
 * POST /msg into the web server. Compose reuses the full-screen editor
 * pattern Settings established (D019): field on top, keyboard below,
 * explicit Send / Cancel.
 *
 * Laid out from the live display size — portrait stacks nothing special,
 * it just gets a taller list (D045).
 */
#include "app_api.h"
#include "app_host.h"
#include "msg.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define COL_BG    lv_color_hex(0x000000)
#define COL_CARD  lv_color_hex(0x161616)
#define COL_TEXT  lv_color_hex(0xE8E8E8)
#define COL_DIM   lv_color_hex(0x8A8A8A)
#define COL_OK    lv_color_hex(0x2FBF71)

static lv_obj_t *scr, *dd_peer, *btn_scan_lbl, *list, *lbl_status;
static lv_obj_t *composer, *comp_ta;
static uint32_t  seen_rev;
static bool      scanning_shown, sending_shown;

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

/* ---------------------------------------------------------------- inbox -- */

static void inbox_refresh(void)
{
    if (!list) return;
    lv_obj_clean(list);
    seen_rev = msg_inbox_rev();

    if (msg_inbox_count() == 0) {
        lv_obj_t *l = lv_label_create(list);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(l, "No messages yet.\nScan finds the other gadgets;"
                             " Write sends to one.");
        return;
    }
    MsgEntry e;
    for (int i = 0; i < msg_inbox_count(); i++) {
        if (!msg_inbox(i, &e)) break;
        lv_obj_t *row = lv_obj_create(list);
        decor(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, COL_CARD, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 10, LV_PART_MAIN);

        char head[48];
        snprintf(head, sizeof(head), "%s  ·  %lus ago", e.from,
                 (unsigned long)((millis() - e.at_ms) / 1000UL));
        lv_obj_t *h = lv_label_create(row);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(h, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(h, head);

        lv_obj_t *t = lv_label_create(row);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(t, COL_TEXT, LV_PART_MAIN);
        lv_obj_set_width(t, LV_PCT(100));
        lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
        lv_label_set_text(t, e.text);
        lv_obj_set_pos(t, 0, 24);
    }
}

static void peers_refresh(void)
{
    if (!dd_peer) return;
    static char opts[MSG_PEERS_N * (MSG_FROM_MAX + 2) + 16];
    opts[0] = '\0';
    char *p = opts;
    MsgPeer pe;
    const int n = msg_peer_count();
    for (int i = 0; i < n; i++) {
        if (!msg_peer(i, &pe)) break;
        p += sprintf(p, i ? "\n%s" : "%s", pe.name);
    }
    if (n == 0) snprintf(opts, sizeof(opts), "no gadgets found");
    lv_dropdown_set_options(dd_peer, opts);
    lv_dropdown_set_selected(dd_peer, 0);
}

/* -------------------------------------------------------------- composer -- */

static void composer_close(void)
{
    if (!composer) return;
    lv_obj_del(composer);
    composer = nullptr; comp_ta = nullptr;
}

static void comp_send_cb(lv_event_t *)
{
    MsgPeer pe;
    if (!msg_peer((int)lv_dropdown_get_selected(dd_peer), &pe)) {
        lv_label_set_text(lbl_status, "no gadget selected - Scan first");
        composer_close();
        return;
    }
    const char *txt = lv_textarea_get_text(comp_ta);
    if (txt && txt[0]) {
        msg_request_send(pe.ip, txt);
        sending_shown = true;
        lv_label_set_text(lbl_status, "sending...");
    }
    composer_close();
}

static void comp_cancel_cb(lv_event_t *) { composer_close(); }

static void comp_kb_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY)  comp_send_cb(nullptr);
    if (c == LV_EVENT_CANCEL) composer_close();
}

static void composer_open(lv_event_t *)
{
    if (composer) return;
    if (msg_peer_count() == 0) {
        lv_label_set_text(lbl_status, "Scan first - nobody to send to");
        return;
    }
    composer = lv_obj_create(scr);
    lv_obj_remove_style_all(composer);
    lv_obj_set_size(composer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(composer, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(composer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(composer, LV_OBJ_FLAG_SCROLLABLE);

    comp_ta = lv_textarea_create(composer);
    lv_obj_set_size(comp_ta, LV_PCT(94), 84);
    lv_obj_align(comp_ta, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(comp_ta, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_textarea_set_max_length(comp_ta, MSG_TEXT_MAX);
    lv_textarea_set_placeholder_text(comp_ta, "message");

    lv_obj_t *send = lv_btn_create(composer);
    lv_obj_set_size(send, 150, 48);
    lv_obj_align(send, LV_ALIGN_TOP_MID, -80, 104);
    lv_obj_set_style_bg_color(send, COL_OK, LV_PART_MAIN);
    lv_obj_add_event_cb(send, comp_send_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *sl = lv_label_create(send);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(sl, LV_SYMBOL_OK "  Send");
    lv_obj_center(sl);

    lv_obj_t *cancel = lv_btn_create(composer);
    lv_obj_set_size(cancel, 150, 48);
    lv_obj_align(cancel, LV_ALIGN_TOP_MID, 80, 104);
    lv_obj_add_event_cb(cancel, comp_cancel_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cl = lv_label_create(cancel);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE "  Cancel");
    lv_obj_center(cl);

    lv_obj_t *kb = lv_keyboard_create(composer);
    lv_obj_set_size(kb, LV_PCT(100), 240);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, comp_ta);
    lv_obj_add_event_cb(kb, comp_kb_cb, LV_EVENT_ALL, nullptr);
}

/* ----------------------------------------------------------------- build -- */

static void scan_cb(lv_event_t *)
{
    if (msg_busy()) return;
    msg_request_scan();
    scanning_shown = true;
    lv_label_set_text(btn_scan_lbl, "...");
    lv_label_set_text(lbl_status, "scanning");
}

static void msgs_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_TRANSP;
    r.border_color = lv_color_hex(0xE8E8E8);
    r.border_width = 5;
    r.radius = 6;
    lv_area_t env = { (lv_coord_t)(cx - 34), (lv_coord_t)(cy - 24),
                      (lv_coord_t)(cx + 34), (lv_coord_t)(cy + 24) };
    lv_draw_rect(ctx, &r, &env);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(0xE8E8E8);
    l.width = 5;
    lv_point_t a = { (lv_coord_t)(cx - 30), (lv_coord_t)(cy - 18) };
    lv_point_t m = { (lv_coord_t)cx,        (lv_coord_t)(cy + 4)  };
    lv_point_t b = { (lv_coord_t)(cx + 30), (lv_coord_t)(cy - 18) };
    lv_draw_line(ctx, &l, &a, &m);
    lv_draw_line(ctx, &l, &m, &b);
}

static lv_obj_t *msgs_create(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    char title[48];
    snprintf(title, sizeof(title), "Messages   ·   I am \"%s\"", msg_name());
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 14, 10);

    dd_peer = lv_dropdown_create(scr);
    lv_obj_set_size(dd_peer, W - 14 - 140 - 10 - 14, 46);
    lv_obj_set_pos(dd_peer, 14, 38);
    lv_obj_set_style_text_font(dd_peer, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd_peer),
                               &lv_font_montserrat_18, LV_PART_MAIN);

    lv_obj_t *sb = lv_btn_create(scr);
    lv_obj_set_size(sb, 140, 46);
    lv_obj_set_pos(sb, W - 14 - 140, 38);
    lv_obj_add_event_cb(sb, scan_cb, LV_EVENT_CLICKED, nullptr);
    btn_scan_lbl = lv_label_create(sb);
    lv_obj_set_style_text_font(btn_scan_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_label_set_text(btn_scan_lbl, LV_SYMBOL_REFRESH "  Scan");
    lv_obj_center(btn_scan_lbl);

    list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, W - 28, H - 96 - 76 - 24);
    lv_obj_set_pos(list, 14, 96);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);

    lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_status, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(lbl_status, "");
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_LEFT, 156, -28);

    app_host_std_back(scr, nullptr);
    lv_obj_t *wr = lv_btn_create(scr);
    lv_obj_set_size(wr, 160, 56);
    lv_obj_align(wr, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_add_event_cb(wr, composer_open, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *wl = lv_label_create(wr);
    lv_obj_set_style_text_font(wl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(wl, LV_SYMBOL_EDIT "  Write");
    lv_obj_center(wl);

    seen_rev = msg_inbox_rev() - 1;   /* force first refresh */
    peers_refresh();
    inbox_refresh();
    return scr;
}

static void msgs_destroy(void)
{
    scr = dd_peer = btn_scan_lbl = list = lbl_status = nullptr;
    composer = comp_ta = nullptr;
    scanning_shown = sending_shown = false;
}

static void msgs_tick(void)
{
    if (!scr) return;
    if (msg_inbox_rev() != seen_rev) inbox_refresh();

    if (scanning_shown && !msg_busy()) {
        scanning_shown = false;
        lv_label_set_text(btn_scan_lbl, LV_SYMBOL_REFRESH "  Scan");
        peers_refresh();
        char st[40];
        snprintf(st, sizeof(st), "%d gadget(s) found", msg_peer_count());
        lv_label_set_text(lbl_status, st);
    }
    if (sending_shown && !msg_busy()) {
        sending_shown = false;
        const int r = msg_last_send_result();
        lv_label_set_text(lbl_status, r == 200 ? "delivered" : "send failed");
        lv_obj_set_style_text_color(lbl_status, r == 200 ? COL_OK : lv_color_hex(0xE0483B),
                                    LV_PART_MAIN);
    }
}

static bool msgs_back(void)
{
    if (composer) { composer_close(); return true; }   /* editor first, D029 */
    return false;
}

extern const App app_messages = { "Messages", msgs_icon, msgs_create, msgs_destroy,
                                  msgs_tick, msgs_back, /*portrait_ok=*/true };
