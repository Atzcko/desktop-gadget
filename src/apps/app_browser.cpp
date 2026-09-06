/**
 * app_browser.cpp — the remote browser (D057).
 *
 * A thin client over the Mac's headless Chrome: a toolbar (back / forward /
 * reload / URL / keyboard), a full-page view blitting browser_frame(), and a
 * touch layer that turns drags into wheel scrolls and taps into clicks. URL
 * editing and page typing each get a keyboard overlay. The heavy lifting —
 * stream decode and input POSTs — lives in browser.cpp's worker tasks; this
 * file only ever runs on the LVGL task.
 */
#include "app_api.h"
#include "app_host.h"
#include "browser.h"

#include <Arduino.h>
#include <string.h>

#define COL_BG    lv_color_hex(0x000000)
#define COL_TEXT  lv_color_hex(0xE8E8E8)
#define COL_DIM   lv_color_hex(0x8A8A8A)
#define COL_CHIP  lv_color_hex(0x1E1E1E)
#define TB        44                    /* toolbar height */

static lv_obj_t   *scr, *page_img, *touch, *url_lbl, *status_lbl;
static lv_obj_t   *url_ov, *url_ta, *kb_ov;
static lv_img_dsc_t frame_dsc;
static uint32_t    seen_rev;
static lv_point_t  press_pt, last_pt;
static int         moved_total, scroll_acc_y, scroll_acc_x;

/* ------------------------------------------------------------- URL editing */
static void url_ov_close(void) { if (url_ov) { lv_obj_del(url_ov); url_ov = nullptr; url_ta = nullptr; } }

static void url_go_cb(lv_event_t *)
{
    if (url_ta) {
        const char *t = lv_textarea_get_text(url_ta);
        if (t && t[0]) browser_nav(t);
    }
    url_ov_close();
}

static void url_edit_cb(lv_event_t *)
{
    if (url_ov) return;
    url_ov = lv_obj_create(scr);
    lv_obj_remove_style_all(url_ov);
    lv_obj_set_size(url_ov, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(url_ov, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(url_ov, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(url_ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *c = lv_label_create(url_ov);
    lv_obj_set_style_text_font(c, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(c, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(c, "Address or search");
    lv_obj_align(c, LV_ALIGN_TOP_LEFT, 14, 12);

    url_ta = lv_textarea_create(url_ov);
    lv_obj_set_size(url_ta, LV_PCT(74), 56);
    lv_obj_align(url_ta, LV_ALIGN_TOP_LEFT, 14, 40);
    lv_obj_set_style_text_font(url_ta, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_textarea_set_one_line(url_ta, true);
    lv_textarea_set_placeholder_text(url_ta, "example.com");

    lv_obj_t *go = lv_btn_create(url_ov);
    lv_obj_set_size(go, LV_PCT(20), 56);
    lv_obj_align(go, LV_ALIGN_TOP_RIGHT, -14, 40);
    lv_obj_set_style_bg_color(go, lv_color_hex(0x2E6BE6), LV_PART_MAIN);
    lv_obj_add_event_cb(go, url_go_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *gl = lv_label_create(go);
    lv_obj_set_style_text_font(gl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(gl, "Go");
    lv_obj_center(gl);

    lv_obj_t *kb = lv_keyboard_create(url_ov);
    lv_keyboard_set_textarea(kb, url_ta);
    lv_obj_add_event_cb(kb, [](lv_event_t *e) {
        const uint32_t c2 = lv_event_get_code(e);
        if (c2 == LV_EVENT_READY) url_go_cb(nullptr);          /* Enter = Go */
    }, LV_EVENT_ALL, nullptr);
}

/* ----------------------------------------------------- page typing keyboard */
static void kb_ov_close(void) { if (kb_ov) { lv_obj_del(kb_ov); kb_ov = nullptr; } }

static void kb_page_cb(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target(e);
    const uint16_t id = lv_keyboard_get_selected_btn(kb);
    const char *txt = lv_keyboard_get_btn_text(kb, id);
    if (!txt) return;
    if      (!strcmp(txt, LV_SYMBOL_BACKSPACE)) browser_input_key("Backspace");
    else if (!strcmp(txt, LV_SYMBOL_NEW_LINE))  browser_input_key("Enter");
    else if (!strcmp(txt, LV_SYMBOL_LEFT))      browser_input_key("ArrowLeft");
    else if (!strcmp(txt, LV_SYMBOL_RIGHT))     browser_input_key("ArrowRight");
    else if (!strcmp(txt, LV_SYMBOL_OK))        kb_ov_close();
    else if (!strcmp(txt, LV_SYMBOL_KEYBOARD))  { /* mode; keyboard handles it */ }
    else if (!strcmp(txt, "ABC") || !strcmp(txt, "abc") || !strcmp(txt, "1#")) { /* mode */ }
    else browser_input_text(txt);               /* a real character (incl. space) */
}

static void kb_toggle_cb(lv_event_t *)
{
    if (kb_ov) { kb_ov_close(); return; }
    kb_ov = lv_obj_create(scr);
    lv_obj_remove_style_all(kb_ov);
    lv_obj_set_size(kb_ov, LV_PCT(100), LV_PCT(50));
    lv_obj_align(kb_ov, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(kb_ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *kb = lv_keyboard_create(kb_ov);
    lv_obj_set_size(kb, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(kb, kb_page_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    /* no textarea: keys go straight to the focused field in Chrome */
}

/* ------------------------------------------------------ page touch handling */
static void page_touch_cb(lv_event_t *e)
{
    const uint32_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p; lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        press_pt = p; last_pt = p;
        moved_total = scroll_acc_x = scroll_acc_y = 0;
    } else if (code == LV_EVENT_PRESSING) {
        const int dx = last_pt.x - p.x;     /* finger left => content scrolls right */
        const int dy = last_pt.y - p.y;     /* finger up   => content scrolls down  */
        scroll_acc_x += dx; scroll_acc_y += dy;
        moved_total += abs(dx) + abs(dy);
        last_pt = p;
        if (abs(scroll_acc_x) >= 8 || abs(scroll_acc_y) >= 8) {
            browser_input_scroll(scroll_acc_x, scroll_acc_y);
            scroll_acc_x = scroll_acc_y = 0;
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (moved_total < 12)               /* a tap, not a drag => click */
            browser_input_tap(press_pt.x, press_pt.y - TB);
    }
}

/* --------------------------------------------------------------- toolbar -- */
static lv_obj_t *tool_btn(int x, int w, const char *sym, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_btn_create(scr);
    lv_obj_set_size(b, w, 36);
    lv_obj_set_pos(b, x, 4);
    lv_obj_set_style_bg_color(b, COL_CHIP, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_label_set_text(l, sym);
    lv_obj_center(l);
    return b;
}

/* a globe: a ring, a meridian, an equator */
static void browser_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;
    const int R = 34;

    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = lv_color_hex(0xE8E8E8);
    a.width = 5;
    lv_point_t c = { (lv_coord_t)cx, (lv_coord_t)cy };
    lv_draw_arc(ctx, &a, &c, R, 0, 360);                 /* the ring */

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(0xE8E8E8);
    l.width = 4;
    lv_point_t e1 = { (lv_coord_t)(cx - R), (lv_coord_t)cy };
    lv_point_t e2 = { (lv_coord_t)(cx + R), (lv_coord_t)cy };
    lv_draw_line(ctx, &l, &e1, &e2);                     /* equator */

    a.width = 4;
    lv_point_t cl = { (lv_coord_t)(cx - R / 2), (lv_coord_t)cy };
    lv_point_t cr = { (lv_coord_t)(cx + R / 2), (lv_coord_t)cy };
    lv_draw_arc(ctx, &a, &cl, R, 300, 60);               /* meridian, left bulge */
    lv_draw_arc(ctx, &a, &cr, R, 120, 240);              /* meridian, right bulge */
}

static lv_obj_t *browser_create(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);
    const int pw = W, ph = H - TB;

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    tool_btn(6,   40, LV_SYMBOL_LEFT,    [](lv_event_t *) { browser_back(); });
    tool_btn(50,  40, LV_SYMBOL_RIGHT,   [](lv_event_t *) { browser_forward(); });
    tool_btn(94,  40, LV_SYMBOL_REFRESH, [](lv_event_t *) { browser_reload(); });
    tool_btn(W-46,40, LV_SYMBOL_KEYBOARD, kb_toggle_cb);

    lv_obj_t *ub = lv_btn_create(scr);
    lv_obj_set_pos(ub, 140, 4);
    lv_obj_set_size(ub, W - 140 - 52, 36);
    lv_obj_set_style_bg_color(ub, lv_color_hex(0x161616), LV_PART_MAIN);
    lv_obj_set_style_radius(ub, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(ub, url_edit_cb, LV_EVENT_CLICKED, nullptr);
    url_lbl = lv_label_create(ub);
    lv_obj_set_style_text_font(url_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(url_lbl, COL_DIM, LV_PART_MAIN);
    lv_label_set_long_mode(url_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(url_lbl, W - 140 - 52 - 16);
    lv_label_set_text(url_lbl, "tap to enter an address");
    lv_obj_center(url_lbl);

    /* the page */
    frame_dsc.header.always_zero = 0;
    frame_dsc.header.w  = pw;
    frame_dsc.header.h  = ph;
    frame_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    frame_dsc.data_size = pw * ph * 2;

    page_img = lv_img_create(scr);
    lv_obj_set_pos(page_img, 0, TB);

    touch = lv_obj_create(scr);
    lv_obj_remove_style_all(touch);
    lv_obj_set_pos(touch, 0, TB);
    lv_obj_set_size(touch, pw, ph);
    lv_obj_add_flag(touch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch, page_touch_cb, LV_EVENT_ALL, nullptr);

    status_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_lbl, COL_DIM, LV_PART_MAIN);
    lv_obj_align(status_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(status_lbl, browser_host()[0] ? "connecting to Chrome..."
                                                    : "set the companion IP (Setup)");

    app_host_std_back(scr, nullptr);            /* bottom-left chip exits the app */

    browser_start(pw, ph);
    seen_rev = 0xFFFFFFFF;
    return scr;
}

static void browser_destroy(void)
{
    browser_stop();
    scr = page_img = touch = url_lbl = status_lbl = nullptr;
    url_ov = url_ta = kb_ov = nullptr;
}

static void browser_tick(void)
{
    if (!scr) return;
    const uint32_t rev = browser_frame_rev();
    if (rev != seen_rev) {
        seen_rev = rev;
        if (browser_streaming() && browser_frame()) {
            frame_dsc.data = (const uint8_t *)browser_frame();
            lv_img_set_src(page_img, &frame_dsc);
            lv_obj_invalidate(page_img);
            if (!lv_obj_has_flag(status_lbl, LV_OBJ_FLAG_HIDDEN))
                lv_obj_add_flag(status_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        const char *st = browser_status();
        if (st[0] && !browser_streaming()) {
            lv_obj_clear_flag(status_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(status_lbl, st);
        }
    }
}

static bool browser_back_nav(void)
{
    if (kb_ov)  { kb_ov_close();  return true; }
    if (url_ov) { url_ov_close(); return true; }
    return false;                               /* fall through: app_host pops the app */
}

extern const App app_browser = { "Browser", browser_icon, browser_create, browser_destroy,
                                 browser_tick, browser_back_nav, /*portrait_ok=*/true };
