/**
 * app_timer.cpp — countdown timer, the first app.
 *
 * It wears the clock: same charcoal cards, same digit font, same split seam,
 * same fold. Setting it uses three gestures on one control, which is what makes
 * a two-number timer bearable on a screen with no keyboard —
 *
 *   drag a card      continuous, coarse
 *   tap top / bottom exactly ±1
 *   hold top /bottom ±1 repeating, ~10/s
 *
 * See D032.
 */
#include "app_api.h"
#include "app_host.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

extern "C" {
LV_FONT_DECLARE(fliqlo_digits);
}

#define COL_BG      lv_color_hex(0x000000)
#define COL_CARD    lv_color_hex(0x161616)
#define COL_PRESS   lv_color_hex(0x262626)
#define COL_DIGIT   lv_color_hex(0xFFFFFF)
#define COL_DIM     lv_color_hex(0x8A8A8A)
#define COL_DONE    lv_color_hex(0xE0483B)

#define CARD_W      268
#define CARD_H      232
#define CARD_GAP    36
#define CARD_RADIUS 26
#define CARD_TOP    30
#define SEAM_H       3          /* same hairline as the clock's big cards */
#define DIGIT_TOP   ((CARD_H - 154) / 2)

#define BTN_TOP     (CARD_TOP + CARD_H + 24)    /* 286 */
#define BTN_H       122                          /* bottom edge at 408 of 450 */

/*
 * 70 ms a phase, not the clock's 180. The clock folds once a minute and the
 * fold is the point; here it fires on every step of a drag, and an animation
 * slower than the finger is exactly what made setting the time feel vague.
 */
#define FOLD_MS       70
#define PX_PER_STEP   18     /* calmer than it was: tap and hold do the fine work */
#define TAP_SLOP      14     /* px before a press stops being a tap              */
#define DRAG_H_SLOP   26     /* px sideways before this stops being a roller     */

struct Digits { lv_obj_t *card, *label, *seam; char text[4]; bool busy; };

static lv_obj_t *scr, *lbl_hint, *btn_play_lbl;
static Digits    d_min, d_sec;

static int      set_seconds  = 5 * 60;
static int      left_seconds = 5 * 60;
static bool     running, finished;
static uint32_t last_tick_ms;

static lv_coord_t drag_x0, drag_y0;
static int        drag_total0;
static bool       dragging, drag_moved;

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

/* ------------------------------------------------------------- the fold -- */
/*
 * A local copy of the clock's two-phase split flap, deliberately not shared.
 * Extracting it from ui.cpp would mean refactoring the one screen that has
 * worked all along in order to add a feature elsewhere; a short duplicate is
 * the cheaper risk. If a third caller ever appears, extract it then.
 */
static void anim_h(void *o, int32_t v) { lv_obj_set_height((lv_obj_t *)o, v); }
static void del_done(lv_anim_t *a)     { lv_obj_del((lv_obj_t *)a->user_data); }
static void clear_busy(lv_anim_t *a)   { ((Digits *)a->user_data)->busy = false; }
static void anim_noop(void *, int32_t) {}

static lv_obj_t *flap(Digits &d, const char *txt, bool top, int h)
{
    lv_obj_t *f = lv_obj_create(d.card);
    decor(f);
    lv_obj_set_size(f, CARD_W, h);
    lv_obj_set_pos(f, 0, top ? 0 : CARD_H / 2);
    lv_obj_set_style_bg_color(f, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *l = lv_label_create(f);
    lv_obj_set_style_text_font(l, &fliqlo_digits, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, finished ? COL_DONE : COL_DIGIT, LV_PART_MAIN);
    lv_label_set_text(l, txt);
    lv_obj_set_width(l, CARD_W);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(l, 0, top ? DIGIT_TOP : DIGIT_TOP - CARD_H / 2);
    return f;
}

static void flip_to(Digits &d, const char *next)
{
    if (!d.card || strcmp(d.text, next) == 0) return;

    char prev[4];
    strncpy(prev, d.text, sizeof(prev));
    prev[sizeof(prev) - 1] = '\0';
    strncpy(d.text, next, sizeof(d.text) - 1);
    d.text[sizeof(d.text) - 1] = '\0';
    lv_label_set_text(d.label, d.text);

    /* Dragging produces changes faster than a fold completes. Stacking them
     * reads as tearing, so a card already mid-fold simply takes the new value
     * and animates the next one. */
    if (d.busy) return;
    d.busy = true;

    lv_obj_t *cover = flap(d, prev,   false, CARD_H / 2);
    lv_obj_t *top   = flap(d, prev,   true,  CARD_H / 2);
    lv_obj_t *bot   = flap(d, d.text, false, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, top);   lv_anim_set_exec_cb(&a, anim_h);
    lv_anim_set_values(&a, CARD_H / 2, 0);
    lv_anim_set_time(&a, FOLD_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_user_data(&a, top);  lv_anim_set_ready_cb(&a, del_done);
    lv_anim_start(&a);

    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, bot);   lv_anim_set_exec_cb(&b, anim_h);
    lv_anim_set_values(&b, 0, CARD_H / 2);
    lv_anim_set_time(&b, FOLD_MS);  lv_anim_set_delay(&b, FOLD_MS);
    lv_anim_set_path_cb(&b, lv_anim_path_ease_out);
    lv_anim_set_user_data(&b, bot);  lv_anim_set_ready_cb(&b, del_done);
    lv_anim_start(&b);

    lv_anim_t c;
    lv_anim_init(&c);
    lv_anim_set_var(&c, cover); lv_anim_set_exec_cb(&c, anim_h);
    lv_anim_set_values(&c, CARD_H / 2, CARD_H / 2);
    lv_anim_set_time(&c, FOLD_MS * 2);
    lv_anim_set_user_data(&c, cover); lv_anim_set_ready_cb(&c, del_done);
    lv_anim_start(&c);

    lv_anim_t f;
    lv_anim_init(&f);
    lv_anim_set_var(&f, d.card); lv_anim_set_exec_cb(&f, anim_noop);
    lv_anim_set_values(&f, 0, 1);
    lv_anim_set_time(&f, FOLD_MS * 2);
    lv_anim_set_user_data(&f, &d);  lv_anim_set_ready_cb(&f, clear_busy);
    lv_anim_start(&f);

    /* The seam belongs on top of the digits, and the flaps were just created
     * above it. Fliqlo's split line crosses the numerals; it is not behind
     * them, and it is not behind the fold either. */
    if (d.seam) lv_obj_move_foreground(d.seam);
}

/* --------------------------------------------------------------- render -- */

static void render(void)
{
    if (!scr) return;
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", left_seconds / 60); flip_to(d_min, buf);
    snprintf(buf, sizeof(buf), "%02d", left_seconds % 60); flip_to(d_sec, buf);

    /* No border, in any state. A coloured frame around a number reads as an
     * error box; the button already says Pause, and the digits are moving. */
    lv_color_t c = finished ? COL_DONE : COL_DIGIT;
    lv_obj_set_style_text_color(d_min.label, c, LV_PART_MAIN);
    lv_obj_set_style_text_color(d_sec.label, c, LV_PART_MAIN);

    lv_label_set_text(btn_play_lbl, running ? LV_SYMBOL_PAUSE "  Pause"
                                            : LV_SYMBOL_PLAY  "  Start");

    if (finished)     lv_label_set_text(lbl_hint, "done");
    else if (running) lv_label_set_text(lbl_hint, "");
    else              lv_label_set_text(lbl_hint, "drag  ·  tap  ·  hold  to set");
}

/* -------------------------------------------------------------- setting -- */
/*
 * Seconds WRAP, minutes clamp. Wrapping makes 59 one tap from 0 in either
 * direction, and it stays reversible during a drag because the value is
 * recomputed from total displacement since touch-down rather than accumulated —
 * so it cannot drift, and dragging back to where you started restores exactly
 * the number you started with.
 */
static void set_from(int minutes, int seconds)
{
    seconds = ((seconds % 60) + 60) % 60;
    if (minutes < 0)  minutes = 0;
    if (minutes > 99) minutes = 99;
    set_seconds  = minutes * 60 + seconds;
    left_seconds = set_seconds;
    render();
}

static void bump(bool is_min, int delta)
{
    if (is_min) set_from(left_seconds / 60 + delta, left_seconds % 60);
    else        set_from(left_seconds / 60, left_seconds % 60 + delta);
}

/* Above the card's middle is up, below it is down. Nothing to aim at, and it
 * works the same whichever card you are on. */
static int tap_dir(bool is_min, lv_coord_t y)
{
    lv_obj_t *card = is_min ? d_min.card : d_sec.card;
    if (!card) return 0;
    lv_area_t a;
    lv_obj_get_coords(card, &a);
    return (y < (a.y1 + a.y2) / 2) ? +1 : -1;
}

static void roller_cb(lv_event_t *e)
{
    if (running || finished) return;
    const bool is_min = (bool)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    switch (code) {
    case LV_EVENT_PRESSED:
        drag_x0     = p.x;
        drag_y0     = p.y;
        drag_total0 = left_seconds;
        dragging    = true;
        drag_moved  = false;
        break;

    case LV_EVENT_PRESSING: {
        if (!dragging) break;
        const int dx = p.x - drag_x0, dy = p.y - drag_y0;

        /* A mostly-sideways drag is the host's back gesture passing through a
         * card, not a roller. Abandon it AND put the number back — swiping
         * home from on top of a card must not leave behind a value nobody
         * chose, and set_seconds outlives the screen. */
        if (abs(dx) > abs(dy) && abs(dx) > DRAG_H_SLOP) {
            dragging = false;
            set_from(drag_total0 / 60, drag_total0 % 60);
            break;
        }
        if (!drag_moved && abs(dy) < TAP_SLOP) break;   /* still a tap */
        drag_moved = true;

        const int steps = (drag_y0 - p.y) / PX_PER_STEP;      /* up = more */
        if (is_min) set_from(drag_total0 / 60 + steps, drag_total0 % 60);
        else        set_from(drag_total0 / 60, drag_total0 % 60 + steps);
        break;
    }

    /* SHORT_CLICKED, not CLICKED: LVGL sends CLICKED on every release
     * including the end of a hold, which would add a stray step to every
     * hold-repeat. */
    case LV_EVENT_SHORT_CLICKED:
        if (!drag_moved) bump(is_min, tap_dir(is_min, p.y));
        break;

    case LV_EVENT_LONG_PRESSED_REPEAT:
        if (!drag_moved) bump(is_min, tap_dir(is_min, p.y));
        break;

    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        dragging = false;
        break;

    default:
        break;
    }
}

/* -------------------------------------------------------------- controls -- */

static void play_cb(lv_event_t *)
{
    if (finished) {
        /* The button says Start. Clearing the alarm and making you press it a
         * second time is not what it says. */
        finished     = false;
        left_seconds = set_seconds;
        running      = true;
    } else if (left_seconds == 0) return;
    else                          running = !running;
    last_tick_ms = millis();
    render();
}

static void reset_cb(lv_event_t *)
{
    running = false; finished = false;
    left_seconds = set_seconds;
    render();
}

/* One level up — the drawer, not the clock. The host owns what "back" means
 * (D033); the button and the edge swipe go through the same door. */
static void back_cb(lv_event_t *) { app_host_back(); }

/* ---------------------------------------------------------------- build -- */
/*
 * The controls are cards too: same charcoal, same radius, same width, sitting
 * directly under the numbers they act on. No seam — that line means "this
 * flips", and drawing it through a word would read as a strikethrough.
 */
static void make_key(int x, int w, const char *txt, lv_event_cb_t cb, lv_obj_t **out_lbl)
{
    lv_obj_t *b = lv_obj_create(scr);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, BTN_H);
    lv_obj_set_pos(b, x, BTN_TOP);
    lv_obj_set_style_bg_color(b, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, COL_PRESS, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, COL_DIGIT, LV_PART_MAIN);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
}

static void build_card(lv_obj_t *parent, Digits &d, int x, bool is_min)
{
    d.card = lv_obj_create(parent);
    lv_obj_remove_style_all(d.card);
    lv_obj_set_size(d.card, CARD_W, CARD_H);
    lv_obj_set_pos(d.card, x, CARD_TOP);
    lv_obj_set_style_bg_color(d.card, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(d.card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(d.card, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(d.card, true, LV_PART_MAIN);
    lv_obj_clear_flag(d.card, LV_OBJ_FLAG_SCROLLABLE);

    /* The card itself is the roller, so it keeps CLICKABLE — the one object
     * on this screen that must NOT go through decor(). */
    lv_obj_add_flag(d.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(d.card, roller_cb, LV_EVENT_ALL, (void *)(intptr_t)is_min);

    d.label = lv_label_create(d.card);
    lv_obj_set_style_text_font(d.label, &fliqlo_digits, LV_PART_MAIN);
    lv_obj_set_style_text_color(d.label, COL_DIGIT, LV_PART_MAIN);
    lv_obj_set_width(d.label, CARD_W);
    lv_obj_set_style_text_align(d.label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(d.label, "00");
    lv_obj_set_pos(d.label, 0, DIGIT_TOP);
    strcpy(d.text, "00");
    d.busy = false;

    /* Created after the label so it draws over the digits. */
    d.seam = lv_obj_create(d.card);
    decor(d.seam);
    lv_obj_set_size(d.seam, CARD_W, SEAM_H);
    lv_obj_set_style_bg_color(d.seam, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(d.seam, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(d.seam, LV_ALIGN_CENTER, 0, 0);
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
    lv_point_t c = { (lv_coord_t)cx, (lv_coord_t)cy };
    lv_draw_arc(ctx, &a, &c, 34, 0, 360);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(0xE8E8E8);
    l.width = 5; l.opa = LV_OPA_COVER;
    l.round_start = 1; l.round_end = 1;
    lv_point_t p1 = { (lv_coord_t)cx, (lv_coord_t)cy };
    lv_point_t p2 = { (lv_coord_t)cx, (lv_coord_t)(cy - 20) };
    lv_draw_line(ctx, &l, &p1, &p2);
    lv_point_t p3 = { (lv_coord_t)(cx + 15), (lv_coord_t)cy };
    lv_draw_line(ctx, &l, &p1, &p3);
}

static lv_obj_t *timer_create(void)
{
    running = false; finished = false; dragging = false; drag_moved = false;
    left_seconds = set_seconds;

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_card(scr, d_min, 14, true);
    build_card(scr, d_sec, 14 + CARD_W + CARD_GAP, false);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *dot = lv_obj_create(scr);
        decor(dot);
        lv_obj_set_size(dot, 14, 14);
        lv_obj_set_style_radius(dot, 7, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, COL_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_pos(dot, 14 + CARD_W + CARD_GAP / 2 - 7,
                       CARD_TOP + (i ? (CARD_H * 2) / 3 : CARD_H / 3) - 7);
    }

    make_key(14,  120, LV_SYMBOL_LEFT, back_cb, nullptr);
    make_key(150, 202, LV_SYMBOL_PLAY    "  Start", play_cb,  &btn_play_lbl);
    make_key(368, 202, LV_SYMBOL_REFRESH "  Reset", reset_cb, nullptr);

    /*
     * The Clock button is gone, so the left-edge swipe is the only way out.
     * A gesture with no affordance is a gesture nobody finds — this is the
     * same thin accent the clock used to show for the hold, doing the same
     * job: say that the edge is live, without spending a line of text on it.
     */
    lv_obj_t *edge = lv_obj_create(scr);
    decor(edge);
    lv_obj_set_size(edge, 4, 92);
    lv_obj_set_style_radius(edge, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(edge, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(edge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(edge, LV_ALIGN_LEFT_MID, 0, 0);

    lbl_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_hint, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(lbl_hint, "");
    lv_obj_align(lbl_hint, LV_ALIGN_TOP_MID, 0, BTN_TOP + BTN_H + 8);

    render();
    return scr;
}

static void timer_destroy(void)
{
    /* The host deletes the screen. Null the pointers so a late tick() can never
     * touch freed objects on the way out. */
    running = false; dragging = false;
    scr = lbl_hint = btn_play_lbl = nullptr;
    d_min.card = d_min.label = d_min.seam = nullptr;
    d_sec.card = d_sec.label = d_sec.seam = nullptr;
}

static void timer_tick(void)
{
    if (!running || !scr) return;
    uint32_t now = millis();
    if (now - last_tick_ms < 1000) return;
    last_tick_ms += 1000;

    if (left_seconds > 0) left_seconds--;
    if (left_seconds == 0) { running = false; finished = true; }
    render();
}

/*
 * `extern` is required, not decoration: in C++ a const object at namespace
 * scope has INTERNAL linkage by default, so without it the registry cannot see
 * this symbol and the link fails with "undefined reference to app_timer".
 */
extern const App app_timer = { "Timer", timer_icon, timer_create, timer_destroy,
                               timer_tick, nullptr };
