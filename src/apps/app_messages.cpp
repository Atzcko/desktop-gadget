/**
 * app_messages.cpp — conversations, the way a phone does them. See D047.
 *
 * Two views inside one app: the CHATS list (one row per peer, last line
 * previewed) and a THREAD (bubbles — theirs left in charcoal, ours right in
 * green). New conversations start by typing a name or an IP; known names
 * resolve from the contact book, which learns from scans, sends, and the
 * source address of everything received (D046).
 *
 * In-app navigation stacks under the host's back: composer -> thread ->
 * list -> (host pops the app). msgs_back() consumes the first three.
 */
#include "app_api.h"
#include "app_host.h"
#include "msg.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define COL_BG      lv_color_hex(0x000000)
#define COL_CARD    lv_color_hex(0x161616)
#define COL_TEXT    lv_color_hex(0xE8E8E8)
#define COL_DIM     lv_color_hex(0x8A8A8A)
#define COL_OK      lv_color_hex(0x2FBF71)
#define COL_BUBBLE_OUT lv_color_hex(0x1E4D36)   /* our side: dark green   */
#define COL_ERR     lv_color_hex(0xE0483B)

static lv_obj_t *scr, *list_view, *thread_view, *overlay;
static lv_obj_t *chat_list, *bubbles, *lbl_status, *thread_title;
static lv_obj_t *ov_ta;
static char      cur_peer[MSG_FROM_MAX + 1];
static uint32_t  seen_rev;
static bool      sending_shown;
static bool      overlay_is_recipient;

static void list_build(void);
static void thread_open(const char *peer);

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

static void age_str(uint32_t at_ms, char *out, size_t cap)
{
    uint32_t s = (millis() - at_ms) / 1000UL;
    if      (s < 60)    snprintf(out, cap, "%lus", (unsigned long)s);
    else if (s < 3600)  snprintf(out, cap, "%lum", (unsigned long)(s / 60));
    else                snprintf(out, cap, "%luh", (unsigned long)(s / 3600));
}

/* ---------------------------------------------------- keyboard overlay -- */
/* One overlay, two jobs: type a recipient (name or IP) or type a message.
 * Field on top, keyboard below, Done/Cancel — the D019 editor pattern. */

static void overlay_close(void)
{
    if (!overlay) return;
    lv_obj_del(overlay);
    overlay = nullptr; ov_ta = nullptr;
}

static void overlay_done(void)
{
    char text[MSG_TEXT_MAX + 1];
    snprintf(text, sizeof(text), "%s", ov_ta ? lv_textarea_get_text(ov_ta) : "");
    const bool was_recipient = overlay_is_recipient;
    overlay_close();
    if (!text[0]) return;

    if (was_recipient) {
        thread_open(text);                    /* named or dotted-quad peer */
        return;
    }
    char ip[16];
    if (!msg_resolve(cur_peer, ip, sizeof(ip))) {
        lv_label_set_text(lbl_status, "no address for this name - Scan, or use an IP");
        lv_obj_set_style_text_color(lbl_status, COL_ERR, LV_PART_MAIN);
        return;
    }
    msg_request_send(ip, text);
    msg_note_sent(cur_peer, ip, text);        /* bubble appears immediately */
    sending_shown = true;
    lv_label_set_text(lbl_status, "sending...");
    lv_obj_set_style_text_color(lbl_status, COL_DIM, LV_PART_MAIN);
}

static void ov_kb_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY)  overlay_done();
    if (c == LV_EVENT_CANCEL) overlay_close();
}

static void overlay_open(bool recipient)
{
    if (overlay) return;
    overlay_is_recipient = recipient;

    overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cap = lv_label_create(overlay);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(cap, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(cap, recipient ? "To: a gadget's name, or its IP"
                                     : cur_peer);
    lv_obj_set_pos(cap, 14, 10);

    ov_ta = lv_textarea_create(overlay);
    lv_obj_set_size(ov_ta, LV_PCT(94), 64);
    lv_obj_align(ov_ta, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_text_font(ov_ta, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_textarea_set_one_line(ov_ta, recipient);
    lv_textarea_set_max_length(ov_ta, recipient ? MSG_FROM_MAX : MSG_TEXT_MAX);
    lv_textarea_set_placeholder_text(ov_ta, recipient ? "Flip Clock 2  /  192.168.0.42"
                                                      : "message");

    lv_obj_t *kb = lv_keyboard_create(overlay);
    lv_obj_set_size(kb, LV_PCT(100), lv_disp_get_ver_res(nullptr) > 500 ? 280 : 240);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ov_ta);
    lv_obj_add_event_cb(kb, ov_kb_cb, LV_EVENT_ALL, nullptr);
}

/* ------------------------------------------------------------- thread -- */

static void bubbles_build(void)
{
    if (!bubbles) return;
    lv_obj_clean(bubbles);
    const int W = lv_disp_get_hor_res(nullptr);

    /* History is newest-first; a thread reads oldest-first. Collect indices
     * of this peer's entries, then emit them in reverse. */
    int idx[MSG_HISTORY_N], n = 0;
    MsgEntry e;
    for (int i = 0; i < msg_history_count(); i++) {
        if (msg_history(i, &e) && strcasecmp(e.peer, cur_peer) == 0) idx[n++] = i;
    }
    if (n == 0) {
        lv_obj_t *l = lv_label_create(bubbles);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(l, "No messages yet - say hello.");
        return;
    }
    for (int k = n - 1; k >= 0; k--) {
        if (!msg_history(idx[k], &e)) continue;

        lv_obj_t *row = lv_obj_create(bubbles);
        decor(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, e.outgoing ? LV_FLEX_ALIGN_END
                                              : LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *bub = lv_obj_create(row);
        decor(bub);
        lv_obj_set_size(bub, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(bub, (W * 3) / 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bub, e.outgoing ? COL_BUBBLE_OUT : COL_CARD,
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bub, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bub, 16, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bub, 12, LV_PART_MAIN);

        lv_obj_t *t = lv_label_create(bub);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(t, COL_TEXT, LV_PART_MAIN);
        lv_obj_set_style_max_width(t, (W * 3) / 4 - 24, LV_PART_MAIN);
        lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
        lv_label_set_text(t, e.text);
    }
    lv_obj_update_layout(bubbles);
    lv_obj_scroll_to_y(bubbles, LV_COORD_MAX, LV_ANIM_OFF);
}

static void thread_close(void)
{
    if (!thread_view) return;
    lv_obj_del(thread_view);
    thread_view = nullptr; bubbles = nullptr; thread_title = nullptr;
    cur_peer[0] = '\0';
    list_build();
    lv_obj_clear_flag(list_view, LV_OBJ_FLAG_HIDDEN);
}

static void thread_open(const char *peer)
{
    snprintf(cur_peer, sizeof(cur_peer), "%s", peer);
    lv_obj_add_flag(list_view, LV_OBJ_FLAG_HIDDEN);

    const int H = lv_disp_get_ver_res(nullptr);

    thread_view = lv_obj_create(scr);
    lv_obj_remove_style_all(thread_view);
    lv_obj_set_size(thread_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(thread_view, LV_OBJ_FLAG_SCROLLABLE);

    thread_title = lv_label_create(thread_view);
    lv_obj_set_style_text_font(thread_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(thread_title, COL_TEXT, LV_PART_MAIN);
    lv_label_set_text(thread_title, cur_peer);
    lv_obj_set_pos(thread_title, 14, 12);

    bubbles = lv_obj_create(thread_view);
    lv_obj_remove_style_all(bubbles);
    lv_obj_set_size(bubbles, LV_PCT(96), H - 44 - 76 - 10);
    lv_obj_align(bubbles, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_flex_flow(bubbles, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bubbles, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(bubbles, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(bubbles, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(bubbles, LV_OBJ_FLAG_CLICKABLE);

    /* Uniform strip: standard back (pops the thread via msgs_back), and the
     * message field beside it, iMessage-shaped. */
    app_host_std_back(thread_view, nullptr);
    lv_obj_t *field = lv_btn_create(thread_view);
    const int W = lv_disp_get_hor_res(nullptr);
    lv_obj_set_size(field, W - 156 - 12, 56);
    lv_obj_align(field, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_set_style_bg_color(field, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_radius(field, 28, LV_PART_MAIN);
    lv_obj_add_event_cb(field, [](lv_event_t *) { overlay_open(false); },
                        LV_EVENT_CLICKED, nullptr);
    lv_obj_t *fl = lv_label_create(field);
    lv_obj_set_style_text_font(fl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(fl, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(fl, "Message...");
    lv_obj_align(fl, LV_ALIGN_LEFT_MID, 18, 0);

    bubbles_build();
}

/* --------------------------------------------------------------- chats -- */

static void chat_row_cb(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    /* user_data is an index into the UNIQUE-PEER order list_build made;
     * rebuild that order the same way to find the name. */
    char peers[MSG_PEERS_N][MSG_FROM_MAX + 1];
    int pn = 0;
    MsgEntry en;
    for (int h = 0; h < msg_history_count() && pn < MSG_PEERS_N; h++) {
        if (!msg_history(h, &en)) break;
        bool known = false;
        for (int k = 0; k < pn; k++)
            if (strcasecmp(peers[k], en.peer) == 0) { known = true; break; }
        if (!known) snprintf(peers[pn++], sizeof(peers[0]), "%s", en.peer);
    }
    if (i < pn) thread_open(peers[i]);
}

static void list_build(void)
{
    if (!chat_list) return;
    lv_obj_clean(chat_list);
    seen_rev = msg_rev();

    /* Conversations: unique peers, newest activity first (history order). */
    char peers[MSG_PEERS_N][MSG_FROM_MAX + 1];
    MsgEntry latest[MSG_PEERS_N];
    int pn = 0;
    MsgEntry e;
    for (int h = 0; h < msg_history_count() && pn < MSG_PEERS_N; h++) {
        if (!msg_history(h, &e)) break;
        bool known = false;
        for (int k = 0; k < pn; k++)
            if (strcasecmp(peers[k], e.peer) == 0) { known = true; break; }
        if (known) continue;
        snprintf(peers[pn], sizeof(peers[0]), "%s", e.peer);
        latest[pn] = e;
        pn++;
    }

    if (pn == 0) {
        lv_obj_t *l = lv_label_create(chat_list);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(l, "No conversations.\n\nNew starts one - type a"
                             " name or an IP.\nScan finds gadgets that are awake.");
        return;
    }
    for (int i = 0; i < pn; i++) {
        lv_obj_t *row = lv_obj_create(chat_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 64);
        lv_obj_set_style_bg_color(row, COL_CARD, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 14, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 10, LV_PART_MAIN);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, chat_row_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        lv_obj_t *nm = lv_label_create(row);
        lv_obj_set_style_text_font(nm, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(nm, COL_TEXT, LV_PART_MAIN);
        lv_label_set_text(nm, peers[i]);
        lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 2, 0);

        char ago[12];
        age_str(latest[i].at_ms, ago, sizeof(ago));
        lv_obj_t *ag = lv_label_create(row);
        lv_obj_set_style_text_font(ag, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(ag, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(ag, ago);
        lv_obj_align(ag, LV_ALIGN_TOP_RIGHT, -2, 0);

        char prev[64];
        snprintf(prev, sizeof(prev), "%s%.44s",
                 latest[i].outgoing ? "you: " : "", latest[i].text);
        lv_obj_t *pv = lv_label_create(row);
        lv_obj_set_style_text_font(pv, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(pv, COL_DIM, LV_PART_MAIN);
        lv_label_set_long_mode(pv, LV_LABEL_LONG_DOT);
        lv_obj_set_width(pv, LV_PCT(100));
        lv_label_set_text(pv, prev);
        lv_obj_align(pv, LV_ALIGN_BOTTOM_LEFT, 2, 0);
    }
}

static void scan_cb(lv_event_t *)
{
    if (msg_busy()) return;
    msg_request_scan();
    lv_label_set_text(lbl_status, "scanning...");
    lv_obj_set_style_text_color(lbl_status, COL_DIM, LV_PART_MAIN);
}

/* ----------------------------------------------------------------- app -- */

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
    const int H = lv_disp_get_ver_res(nullptr);
    const int W = lv_disp_get_hor_res(nullptr);

    msg_mark_read();                 /* opening the app clears the badge */

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    list_view = lv_obj_create(scr);
    lv_obj_remove_style_all(list_view);
    lv_obj_set_size(list_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(list_view, LV_OBJ_FLAG_SCROLLABLE);

    char title[48];
    snprintf(title, sizeof(title), "Messages   ·   I am \"%s\"", msg_name());
    lv_obj_t *t = lv_label_create(list_view);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 14, 12);

    chat_list = lv_obj_create(list_view);
    lv_obj_remove_style_all(chat_list);
    lv_obj_set_size(chat_list, LV_PCT(96), H - 44 - 76 - 10);
    lv_obj_align(chat_list, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_flex_flow(chat_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(chat_list, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(chat_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(chat_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(chat_list, LV_OBJ_FLAG_CLICKABLE);

    lbl_status = lv_label_create(list_view);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_status, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(lbl_status, "");
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_LEFT, 156, -74);

    app_host_std_back(list_view, nullptr);

    lv_obj_t *nw = lv_btn_create(list_view);
    lv_obj_set_size(nw, (W - 156 - 12 - 8) / 2, 56);
    lv_obj_align(nw, LV_ALIGN_BOTTOM_RIGHT, -12 - (W - 156 - 12 - 8) / 2 - 8, -10);
    lv_obj_set_style_bg_color(nw, COL_BUBBLE_OUT, LV_PART_MAIN);
    lv_obj_add_event_cb(nw, [](lv_event_t *) { overlay_open(true); },
                        LV_EVENT_CLICKED, nullptr);
    lv_obj_t *nl = lv_label_create(nw);
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(nl, LV_SYMBOL_EDIT "  New");
    lv_obj_center(nl);

    lv_obj_t *sc = lv_btn_create(list_view);
    lv_obj_set_size(sc, (W - 156 - 12 - 8) / 2, 56);
    lv_obj_align(sc, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_add_event_cb(sc, scan_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *sl = lv_label_create(sc);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(sl, LV_SYMBOL_REFRESH "  Scan");
    lv_obj_center(sl);

    thread_view = nullptr; overlay = nullptr; cur_peer[0] = '\0';
    list_build();
    return scr;
}

static void msgs_destroy(void)
{
    scr = list_view = thread_view = overlay = nullptr;
    chat_list = bubbles = lbl_status = thread_title = ov_ta = nullptr;
    cur_peer[0] = '\0';
    sending_shown = false;
}

static void msgs_tick(void)
{
    if (!scr) return;

    if (msg_rev() != seen_rev) {
        seen_rev = msg_rev();
        msg_mark_read();             /* you are looking at the app */
        if (thread_view && cur_peer[0]) bubbles_build();
        else if (!thread_view)          list_build();
    }
    if (sending_shown && !msg_busy()) {
        sending_shown = false;
        const int r = msg_last_send_result();
        if (r == 200) {
            lv_label_set_text(lbl_status, "");
        } else {
            lv_label_set_text(lbl_status, "delivery failed - gadget offline?");
            lv_obj_set_style_text_color(lbl_status, COL_ERR, LV_PART_MAIN);
        }
    }
}

static bool msgs_back(void)
{
    if (overlay)     { overlay_close(); return true; }
    if (thread_view) { thread_close();  return true; }
    return false;
}

extern const App app_messages = { "Messages", msgs_icon, msgs_create, msgs_destroy,
                                  msgs_tick, msgs_back, /*portrait_ok=*/true };
