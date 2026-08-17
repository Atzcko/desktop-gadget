/**
 * app_timer.cpp — countdown timer, the first app.
 *
 * Set by dragging up or down anywhere on the face; play / pause / reset / back.
 * Uses the same charcoal cards and the same 210 px digits as the clock, because
 * a second visual language on one device would be one too many.
 */
#include "app_api.h"
#include "app_host.h"

#include <Arduino.h>
#include <stdio.h>

extern "C" {
LV_FONT_DECLARE(fliqlo_digits);
}

#define COL_BG      lv_color_hex(0x000000)
#define COL_CARD    lv_color_hex(0x161616)
#define COL_DIGIT   lv_color_hex(0xFFFFFF)
#define COL_DIM     lv_color_hex(0x8A8A8A)
#define COL_RUN     lv_color_hex(0x2FBF71)
#define COL_DONE    lv_color_hex(0xE0483B)

#define CARD_W      268
#define CARD_H      232
#define CARD_GAP    36
#define CARD_RADIUS 26
#define DIGIT_TOP   ((CARD_H - 154) / 2)

#define MAX_SECONDS (99 * 60 + 59)
#define PX_PER_STEP 8          /* drag sensitivity: 8 px = one step */

static lv_obj_t *scr, *lbl_min, *lbl_sec, *lbl_hint, *btn_play_lbl;
static lv_obj_t *card_a, *card_b;

static int      set_seconds  = 5 * 60;   /* what reset returns to */
static int      left_seconds = 5 * 60;
static bool     running;
static bool     finished;
static uint32_t last_tick_ms;

/* Drag state */
static lv_coord_t drag_start_y;
static int        drag_start_val;
static bool       dragging;

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

static void render(void)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", left_seconds / 60);
    lv_label_set_text(lbl_min, buf);
    snprintf(buf, sizeof(buf), "%02d", left_seconds % 60);
    lv_label_set_text(lbl_sec, buf);

    lv_color_t c = finished ? COL_DONE : COL_DIGIT;
    lv_obj_set_style_text_color(lbl_min, c, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_sec, c, LV_PART_MAIN);

    lv_obj_set_style_border_width(card_a, running || finished ? 4 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(card_b, running || finished ? 4 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(card_a, finished ? COL_DONE : COL_RUN, LV_PART_MAIN);
    lv_obj_set_style_border_color(card_b, finished ? COL_DONE : COL_RUN, LV_PART_MAIN);

    lv_label_set_text(btn_play_lbl, running ? LV_SYMBOL_PAUSE "  Pause"
                                            : LV_SYMBOL_PLAY  "  Start");
    lv_label_set_text(lbl_hint,
        finished ? "done"
                 : (running ? "" : "drag up or down to set"));
}

/*
 * Dragging sets the time, but ONLY while stopped — adjusting a running
 * countdown by brushing the screen would be a trap. The value is computed from
 * the total displacement since touch-down rather than accumulated per event, so
 * it cannot drift and a drag back to where it started restores the original.
 */
static void face_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (running || finished) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        drag_start_y   = p.y;
        drag_start_val = set_seconds;
        dragging       = true;
    } else if (code == LV_EVENT_PRESSING && dragging) {
        int steps = (drag_start_y - p.y) / PX_PER_STEP;      /* up = more */
        /* Coarse above ten minutes: minutes there, 15 s below, so both a
         * 45-second egg and a 40-minute bake are reachable in one gesture. */
        int v = drag_start_val + (abs(drag_start_val) >= 600 ? steps * 60 : steps * 15);
        if (v < 0) v = 0;
        if (v > MAX_SECONDS) v = MAX_SECONDS;
        set_seconds = v;
        left_seconds = v;
        render();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        dragging = false;
    }
}

static void play_cb(lv_event_t *)
{
    if (finished) { finished = false; left_seconds = set_seconds; }
    else if (left_seconds == 0) return;
    else running = !running;
    last_tick_ms = millis();
    render();
}

static void reset_cb(lv_event_t *)
{
    running = false;
    finished = false;
    left_seconds = set_seconds;
    render();
}

static void back_cb(lv_event_t *) { app_host_home(); }

static lv_obj_t *make_btn(lv_obj_t *parent, const char *txt, lv_event_cb_t cb,
                          int x, int w, lv_obj_t **out_lbl)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, 50);
    lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, x, -18);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    return b;
}

static lv_obj_t *card(lv_obj_t *parent, int x, lv_obj_t **out_lbl)
{
    lv_obj_t *c = lv_obj_create(parent);
    decor(c);
    lv_obj_set_size(c, CARD_W, CARD_H);
    lv_obj_set_pos(c, x, 30);
    lv_obj_set_style_bg_color(c, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(c, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(c, true, LV_PART_MAIN);

    lv_obj_t *l = lv_label_create(c);
    lv_obj_set_style_text_font(l, &fliqlo_digits, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, COL_DIGIT, LV_PART_MAIN);
    lv_obj_set_width(l, CARD_W);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(l, "00");
    lv_obj_set_pos(l, 0, DIGIT_TOP);
    *out_lbl = l;
    return c;
}

static void timer_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = lv_color_hex(0xE8E8E8);
    a.width = 5;
    a.opa   = LV_OPA_COVER;
    lv_point_t centre = { (lv_coord_t)cx, (lv_coord_t)cy };
    lv_draw_arc(ctx, &a, &centre, 34, 0, 360);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(0xE8E8E8);
    l.width = 5;
    l.opa   = LV_OPA_COVER;
    l.round_start = 1; l.round_end = 1;
    lv_point_t p1 = { (lv_coord_t)cx, (lv_coord_t)cy };
    lv_point_t p2 = { (lv_coord_t)cx, (lv_coord_t)(cy - 20) };
    lv_draw_line(ctx, &l, &p1, &p2);
    lv_point_t p3 = { (lv_coord_t)(cx + 15), (lv_coord_t)cy };
    lv_draw_line(ctx, &l, &p1, &p3);
}

static lv_obj_t *timer_create(void)
{
    running = false;
    finished = false;
    left_seconds = set_seconds;

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* The face is the drag surface. It sits under the buttons and above
     * nothing, so the buttons still get their own clicks. */
    lv_obj_t *face = lv_obj_create(scr);
    lv_obj_remove_style_all(face);
    lv_obj_set_size(face, 600, 300);
    lv_obj_set_pos(face, 0, 0);
    lv_obj_add_flag(face, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(face, face_cb, LV_EVENT_ALL, nullptr);

    card_a = card(face, 14, &lbl_min);
    card_b = card(face, 14 + CARD_W + CARD_GAP, &lbl_sec);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *d = lv_obj_create(face);
        decor(d);
        lv_obj_set_size(d, 14, 14);
        lv_obj_set_style_radius(d, 7, LV_PART_MAIN);
        lv_obj_set_style_bg_color(d, COL_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_pos(d, 14 + CARD_W + CARD_GAP / 2 - 7,
                       30 + (i ? (CARD_H * 2) / 3 : CARD_H / 3) - 7);
    }

    lbl_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_hint, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(lbl_hint, "");
    lv_obj_align(lbl_hint, LV_ALIGN_TOP_MID, 0, 280);

    make_btn(scr, LV_SYMBOL_PLAY "  Start", play_cb,  30, 200, &btn_play_lbl);
    make_btn(scr, LV_SYMBOL_REFRESH "  Reset", reset_cb, 244, 150, nullptr);
    make_btn(scr, LV_SYMBOL_LEFT "  Clock", back_cb, 408, 162, nullptr);

    render();
    return scr;
}

static void timer_destroy(void)
{
    /* The host deletes the screen; just drop our pointers so a stale one can
     * never be dereferenced by tick() on the way out. */
    running = false;
    scr = lbl_min = lbl_sec = lbl_hint = btn_play_lbl = nullptr;
    card_a = card_b = nullptr;
}

static void timer_tick(void)
{
    if (!running || !lbl_min) return;
    uint32_t now = millis();
    if (now - last_tick_ms < 1000) return;
    last_tick_ms += 1000;

    if (left_seconds > 0) left_seconds--;
    if (left_seconds == 0) { running = false; finished = true; }
    render();
}

/*
 * `extern` is required, not decoration. In C++ a const object at namespace
 * scope has INTERNAL linkage by default, so without it this symbol is invisible
 * to the registry and the link fails with "undefined reference to app_timer".
 */
extern const App app_timer = { "Timer", timer_icon, timer_create, timer_destroy, timer_tick };
