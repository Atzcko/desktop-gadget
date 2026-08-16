/**
 * ui.cpp — the Fliqlo layout, the split-flap fold, and touch gestures.
 *
 * Aesthetic rules from the brief: pure #000000 background so AMOLED pixels
 * are genuinely off; two rounded charcoal cards with a horizontal centre
 * huge white digits; nothing on screen but the clock and weather.
 */
#include "ui.h"
#include "config.h"
#include "settings.h"
#include "ui_settings.h"
#include "app.h"
#include "emotion.h"

#include <Arduino.h>
#include <stdio.h>
#include <math.h>

extern "C" {
LV_FONT_DECLARE(fliqlo_digits);
LV_FONT_DECLARE(fliqlo_corner);
LV_FONT_DECLARE(fliqlo_mid);
}

#define COL_BG          lv_color_hex(0x000000)
#define COL_CARD        lv_color_hex(0x161616)
#define COL_DIGIT       lv_color_hex(0xFFFFFF)
#define COL_COLON       lv_color_hex(0x707070)
#define COL_TEMP        lv_color_hex(0xFFFFFF)
#define COL_SECONDARY   lv_color_hex(0x8A8A8A)
/* Humidity is deliberately the quietest thing on the panel: same 28 px size
 * as min/max — introducing a third type size would break the brief's cap —
 * so it recedes by CONTRAST instead. Roughly half the luminance of the
 * min/max grey, which reads as present-but-secondary rather than as
 * another number competing for attention. */
#define COL_TERTIARY    lv_color_hex(0x4E4E4E)
#define COL_ACCENT      lv_color_hex(0x3C7DD9)
#define COL_STALE       lv_color_hex(0x8A6A2A)

/* See D012 — every constant derives from the 116.8 px digit advance, and
 * every one of them is even so LVGL's even-coordinate rounder never
 * enlarges a redraw region. */
#define CARD_W          268
#define CARD_H          232
#define CARD_GAP        36
#define CARD_RADIUS     26
#define SEAM_H           3      /* big cards only */
#define COLON_DOT       14
#define LABEL_H         154      /* fliqlo_digits line_height */
#define DIGIT_TOP       ((CARD_H - LABEL_H) / 2)

#define FOLD_MS         180      /* per phase; two phases per flip */

/* Gesture thresholds */
#define TAP_MAX_MS      400
#define BRIGHT_MIN_MS   1200
#define SETTINGS_MS     3000     /* hold-to-open-Settings */

/*
 * SEAM: big cards only, and thin.
 *
 * It reads correctly at 232 px, where 2 px is a hairline that sits cleanly and
 * says "split-flap". It does NOT survive being scaled: the same card language
 * now also runs at 55 px (corner clock) and 58 px (weather), and a line across
 * a 55 px card has too few pixels to land on and reads as choppy — so the
 * small cards have none.
 *
 * The fold animation never depended on it. It hinges at CARD_H/2 regardless;
 * the seam is the resting hint of where that hinge is.
 */
struct Card {
    lv_obj_t *root;
    lv_obj_t *label;
    lv_obj_t *seam;
    char      text[4];
};

static lv_obj_t *scr_clock;
static lv_obj_t *root;
static lv_obj_t *clock_grp, *weather_grp, *strip_grp;
static lv_obj_t *zoom_canvas;
static lv_color_t *zoom_buf;
#define ZOOM_W (CARD_W * 2 + CARD_GAP)
#define ZOOM_H CARD_H

/* The strip shown in line mode: the same charcoal cards, the
 * same colon — scaled down. LVGL 8 cannot transform text, so "smaller" means
 * a second compiled face (fliqlo_small, 38 px) rather than a zoom. */
/*
 * The corner clock is a TRUE SCALE of the big one, not a smaller lookalike.
 * Zoom 61/256 = 0.2381, chosen so the 210 px digits land at exactly 50 px —
 * which is why fliqlo_corner is 50 px. Every dimension below is the big
 * card's dimension multiplied by that same factor, so when the zooming canvas
 * hands over to these real cards nothing moves or changes size.
 *
 *   card   268 x 232 -> 64 x 55      radius 26 -> 6
 *   gap    36        -> 9            row 572  -> 137
 *   pad    17.2      -> 4.1          == (64 - 2*27.8)/2 = 4.1   [matches]
 */
#define ZOOM_END     61      /* 256 == 1:1 */
#define MINI_W       64
#define MINI_H       55
#define MINI_RADIUS   6
#define MINI_GAP      9
#define MINI_X       14
#define MINI_Y       14

#define WX_H         58      /* resting weather cards, 44 px face */
#define WX_RADIUS    10
#define WX_PAD_X     11
#define WX_Y        330

struct MiniCard { lv_obj_t *root, *label; };
static MiniCard m_hh, m_mm;                            /* corner clock     */
static MiniCard w_temp, w_minmax, w_hum;               /* resting weather  */
static Card      card_h, card_m;
static lv_obj_t *lbl_ampm;
static lv_obj_t *dot_stale;
static lv_obj_t *lbl_info;
static lv_obj_t *hold_bar;
static lv_timer_t *info_timer;

static uint16_t scr_w, scr_h;
static int      clock_x, clock_y;

/*
 * Every lv_obj_create() starts life CLICKABLE — lv_obj_constructor sets the
 * flag unconditionally — and LVGL 8 does NOT bubble input events by default
 * (LV_OBJ_FLAG_EVENT_BUBBLE is absent from the constructor's flag set). A
 * purely decorative child would therefore hit-test first and swallow the
 * press before it ever reached the root gesture handler, leaving the
 * hold-to-Settings gesture working only in the thin margins around the cards.
 *
 * Strip the flag from anything that is only there to be looked at.
 */
static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    /* SCROLL_CHAIN is also set by the constructor whenever the object has a
     * parent. Leaving it on means a drag that this object does not handle is
     * forwarded UP the parent chain until something scrollable accepts it —
     * which is how the whole clock became draggable. */
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

/* ------------------------------------------------------------ the fold -- */

static void anim_height_cb(void *obj, int32_t v)
{
    lv_obj_set_height((lv_obj_t *)obj, v);
}

static void del_on_done(lv_anim_t *a)
{
    lv_obj_del((lv_obj_t *)a->user_data);
}

/* A half-card panel carrying a copy of the digits, aligned so the glyphs
 * land exactly where they sit in the full card. `top_half` picks which
 * half of the card the panel represents. */
static lv_obj_t *make_flap(Card &c, const char *text, bool top_half, int height)
{
    lv_obj_t *flap = lv_obj_create(c.root);
    decor(flap);
    lv_obj_set_size(flap, CARD_W, height);
    lv_obj_set_pos(flap, 0, top_half ? 0 : CARD_H / 2);
    lv_obj_set_style_bg_color(flap, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(flap, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(flap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(flap);
    lv_obj_set_style_text_font(l, &fliqlo_digits, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, COL_DIGIT, LV_PART_MAIN);
    lv_label_set_text(l, text);
    lv_obj_set_width(l, CARD_W);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    /* Children are clipped to the flap, so the bottom flap's label simply
     * sits at a negative y and the top of it is cut away. */
    lv_obj_set_pos(l, 0, top_half ? DIGIT_TOP : DIGIT_TOP - CARD_H / 2);

    return flap;
}

/*
 * Classic two-phase split-flap:
 *   phase A — the OLD top folds down to nothing, revealing the NEW top
 *             underneath, while a static cover keeps the OLD bottom visible
 *   phase B — the NEW bottom grows from the hinge downward, covering it
 */
static void flip_card(Card &c, const char *next)
{
    if (strcmp(c.text, next) == 0) return;

    char prev[4];
    strncpy(prev, c.text, sizeof(prev));
    strncpy(c.text, next, sizeof(c.text) - 1);
    c.text[sizeof(c.text) - 1] = '\0';

    /* The underlying label becomes the new value immediately; the flaps
     * are what sell the transition. */
    lv_label_set_text(c.label, c.text);

    lv_obj_t *cover_bottom = make_flap(c, prev, false, CARD_H / 2);
    lv_obj_t *flap_top     = make_flap(c, prev, true,  CARD_H / 2);
    lv_obj_move_foreground(c.seam);

    /* Phase A */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, flap_top);
    lv_anim_set_exec_cb(&a, anim_height_cb);
    lv_anim_set_values(&a, CARD_H / 2, 0);
    lv_anim_set_time(&a, FOLD_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_user_data(&a, flap_top);
    lv_anim_set_ready_cb(&a, del_on_done);
    lv_anim_start(&a);

    /* Phase B, delayed by exactly phase A's duration. */
    lv_obj_t *flap_bottom = make_flap(c, c.text, false, 0);
    lv_obj_move_foreground(c.seam);

    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, flap_bottom);
    lv_anim_set_exec_cb(&b, anim_height_cb);
    lv_anim_set_values(&b, 0, CARD_H / 2);
    lv_anim_set_time(&b, FOLD_MS);
    lv_anim_set_delay(&b, FOLD_MS);
    lv_anim_set_path_cb(&b, lv_anim_path_ease_out);
    lv_anim_set_user_data(&b, flap_bottom);
    lv_anim_set_ready_cb(&b, del_on_done);
    lv_anim_start(&b);

    /* The static cover disappears once the new bottom has covered it. */
    lv_anim_t c2;
    lv_anim_init(&c2);
    lv_anim_set_var(&c2, cover_bottom);
    lv_anim_set_exec_cb(&c2, anim_height_cb);
    lv_anim_set_values(&c2, CARD_H / 2, CARD_H / 2);
    lv_anim_set_time(&c2, FOLD_MS * 2);
    lv_anim_set_user_data(&c2, cover_bottom);
    lv_anim_set_ready_cb(&c2, del_on_done);
    lv_anim_start(&c2);
}

/* ------------------------------------------------------------- gesture -- */

static uint32_t press_start;
static bool     press_active;
static bool     settings_fired;

static void hide_info_cb(lv_timer_t *t)
{
    lv_obj_add_flag(lbl_info, LV_OBJ_FLAG_HIDDEN);
    info_timer = nullptr;
    lv_timer_del(t);
}

static void cycle_brightness(void)
{
    static const uint8_t steps[] = { 25, 60, 90, 140, 200 };
    Settings &s = settings_get();
    int idx = 0;
    for (int i = 0; i < (int)(sizeof(steps)); i++) {
        if (steps[i] == s.brightness_day) { idx = i; break; }
    }
    idx = (idx + 1) % (int)(sizeof(steps));
    s.brightness_day = steps[idx];
    settings_save();
    app_apply_brightness(s.brightness_day);
}

static void on_press(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        press_start    = millis();
        press_active   = true;
        settings_fired = false;
    } else if (code == LV_EVENT_PRESSING && press_active) {
        uint32_t dt = millis() - press_start;
        if (dt >= BRIGHT_MIN_MS && !settings_fired) {
            /* Show the user that holding longer does something. Without
             * this, a 3 s hold is an invisible affordance nobody finds. */
            lv_obj_clear_flag(hold_bar, LV_OBJ_FLAG_HIDDEN);
            int span = SETTINGS_MS - BRIGHT_MIN_MS;
            int w    = (int)(240L * (long)(dt - BRIGHT_MIN_MS) / span);
            lv_obj_set_width(hold_bar, w < 2 ? 2 : (w > 240 ? 240 : w));
        }
        if (dt >= SETTINGS_MS && !settings_fired) {
            settings_fired = true;
            press_active   = false;
            lv_obj_add_flag(hold_bar, LV_OBJ_FLAG_HIDDEN);
            ui_settings_open();
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_add_flag(hold_bar, LV_OBJ_FLAG_HIDDEN);
        if (!press_active) { press_active = false; return; }
        uint32_t dt  = millis() - press_start;
        press_active = false;
        if (settings_fired) return;

        if (dt < TAP_MAX_MS)          app_show_info_overlay();
        else if (dt >= BRIGHT_MIN_MS) cycle_brightness();
    }
}

/* ---------------------------------------------------------- build the UI -- */

static void make_card(Card &c, int x, int y)
{
    c.root = lv_obj_create(clock_grp);
    decor(c.root);
    lv_obj_set_size(c.root, CARD_W, CARD_H);
    lv_obj_set_pos(c.root, x, y);
    lv_obj_set_style_bg_color(c.root, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(c.root, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(c.root, true, LV_PART_MAIN);
    lv_obj_clear_flag(c.root, LV_OBJ_FLAG_SCROLLABLE);

    c.label = lv_label_create(c.root);
    lv_obj_set_style_text_font(c.label, &fliqlo_digits, LV_PART_MAIN);
    lv_obj_set_style_text_color(c.label, COL_DIGIT, LV_PART_MAIN);
    lv_obj_set_width(c.label, CARD_W);
    lv_obj_set_style_text_align(c.label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(c.label, "00");
    lv_obj_set_pos(c.label, 0, DIGIT_TOP);
    strcpy(c.text, "00");

    /* Created last so it draws over the digits — in Fliqlo the split line
     * crosses the numerals, it is not behind them. */
    c.seam = lv_obj_create(c.root);
    decor(c.seam);
    lv_obj_set_size(c.seam, CARD_W, SEAM_H);
    lv_obj_set_style_bg_color(c.seam, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c.seam, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(c.seam, LV_ALIGN_CENTER, 0, 0);
}

/* One card, at whatever scale is asked for. Every value on this device now
 * wears the same charcoal rounded card — only the size
 * changes between the clock, the weather row and the line-mode strip. */
static void mini_card(lv_obj_t *parent, MiniCard &c, const lv_font_t *font,
                      lv_coord_t h, lv_coord_t radius, lv_color_t colour,
                      const char *init)
{
    c.root = lv_obj_create(parent);
    decor(c.root);
    lv_obj_set_size(c.root, 60, h);               /* real width set by card_sync */
    lv_obj_set_style_bg_color(c.root, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(c.root, radius, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(c.root, true, LV_PART_MAIN);

    c.label = lv_label_create(c.root);
    lv_obj_set_style_text_font(c.label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(c.label, colour, LV_PART_MAIN);
    lv_label_set_text(c.label, init);
    lv_obj_center(c.label);

}

/*
 * Cards size themselves to their text, which changes ("5°" vs "34°"). Measure
 * the label after layout and set the card explicitly rather than using
 * LV_SIZE_CONTENT — deterministic, and flex re-centres the row afterwards.
 */
static void card_sync(lv_obj_t *group, MiniCard **cards, int n,
                      lv_coord_t h, lv_coord_t pad)
{
    if (!group) return;
    lv_obj_update_layout(group);
    for (int i = 0; i < n; i++) {
        lv_coord_t w = lv_obj_get_width(cards[i]->label) + 2 * pad;
        lv_obj_set_size(cards[i]->root, w, h);
    }
}

static void weather_sync(void)
{
    MiniCard *c[3] = { &w_temp, &w_minmax, &w_hum };
    card_sync(weather_grp, c, 3, WX_H, WX_PAD_X);
}

void ui_init(uint16_t screen_w, uint16_t screen_h)
{
    scr_w = screen_w;
    scr_h = screen_h;

    scr_clock = lv_scr_act();
    lv_obj_set_style_bg_color(scr_clock, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_clock, LV_OPA_COVER, LV_PART_MAIN);

    /*
     * The clock is a fixed layout — it must never move under a finger.
     * lv_scr_act() is an ordinary lv_obj and therefore SCROLLABLE by
     * default, so a drag anywhere scrolled the entire screen. Nothing here
     * is ever meant to scroll; the only thing that legitimately moves the
     * layout is the burn-in walk, which sets a position directly.
     */
    lv_obj_clear_flag(scr_clock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_clock, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(scr_clock, 0, LV_PART_MAIN);

    root = lv_obj_create(scr_clock);
    decor(root);
    lv_obj_set_size(root, screen_w, screen_h);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, on_press, LV_EVENT_ALL, nullptr);

    /* One container for the whole clock so the emotion transition can fade
     * and slide it with a single animation instead of five. */
    clock_grp = lv_obj_create(root);
    decor(clock_grp);
    lv_obj_set_size(clock_grp, screen_w, screen_h);
    lv_obj_set_pos(clock_grp, 0, 0);

    const int clock_w = CARD_W * 2 + CARD_GAP;
    clock_x = (screen_w - clock_w) / 2;
    clock_y = 44;

    make_card(card_h, clock_x, clock_y);
    make_card(card_m, clock_x + CARD_W + CARD_GAP, clock_y);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *dot = lv_obj_create(clock_grp);
        decor(dot);
        lv_obj_set_size(dot, COLON_DOT, COLON_DOT);
        lv_obj_set_style_bg_color(dot, COL_COLON, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(dot, COLON_DOT / 2, LV_PART_MAIN);
        int dy = (i == 0) ? CARD_H / 3 : (CARD_H * 2) / 3;
        lv_obj_set_pos(dot, clock_x + CARD_W + CARD_GAP / 2 - COLON_DOT / 2,
                       clock_y + dy - COLON_DOT / 2);
    }

    /* AM/PM marker, only ever visible in 12-hour mode so the default
     * 24-hour aesthetic stays exactly as the brief specifies. */
    lbl_ampm = lv_label_create(card_h.root);
    lv_obj_set_style_text_font(lbl_ampm, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_ampm, COL_SECONDARY, LV_PART_MAIN);
    lv_label_set_text(lbl_ampm, "");
    lv_obj_align(lbl_ampm, LV_ALIGN_BOTTOM_RIGHT, -14, -10);
    lv_obj_add_flag(lbl_ampm, LV_OBJ_FLAG_HIDDEN);

    /* Weather: largest element is the current temperature, min/max beside
     * it in the one smaller size. Two sizes total, per the brief. */
    /*
     * Resting weather: the same charcoal cards as the clock, at 44 px.
     * Every value on this screen now wears the same card —
     * nothing is a stray text label. Hierarchy is carried by scale (the clock
     * is 4.8x the type size) and by colour, not by two different treatments.
     */
    weather_grp = lv_obj_create(root);
    decor(weather_grp);
    lv_obj_set_size(weather_grp, screen_w, WX_H);
    lv_obj_set_pos(weather_grp, 0, WX_Y);
    lv_obj_set_flex_flow(weather_grp, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weather_grp, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(weather_grp, 10, LV_PART_MAIN);

    mini_card(weather_grp, w_temp,   &fliqlo_mid, WX_H, WX_RADIUS,
              COL_TEMP,      "--" WEATHER_UNIT_SUFFIX);
    mini_card(weather_grp, w_minmax, &fliqlo_mid, WX_H, WX_RADIUS,
              lv_color_hex(0x9A9A9A), "--/--");
    mini_card(weather_grp, w_hum,    &fliqlo_mid, WX_H, WX_RADIUS,
              lv_color_hex(0x6E6E6E), "--%");

    /* Stale marker rides beside the cards — still a shape, not a type size. */
    dot_stale = lv_obj_create(weather_grp);
    decor(dot_stale);
    lv_obj_set_size(dot_stale, 10, 10);
    lv_obj_set_style_bg_color(dot_stale, COL_STALE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot_stale, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dot_stale, 5, LV_PART_MAIN);
    lv_obj_add_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);

    weather_sync();

    /* Corner clock for line mode — the landing place for the zoom. */
    strip_grp = lv_obj_create(root);
    decor(strip_grp);
    lv_obj_set_size(strip_grp, MINI_W * 2 + MINI_GAP, MINI_H);
    lv_obj_set_pos(strip_grp, MINI_X, MINI_Y);
    lv_obj_set_style_opa(strip_grp, LV_OPA_TRANSP, LV_PART_MAIN);

    mini_card(strip_grp, m_hh, &fliqlo_corner, MINI_H, MINI_RADIUS, COL_DIGIT, "00");
    mini_card(strip_grp, m_mm, &fliqlo_corner, MINI_H, MINI_RADIUS, COL_DIGIT, "00");
    lv_obj_set_size(m_hh.root, MINI_W, MINI_H);
    lv_obj_set_pos(m_hh.root, 0, 0);
    lv_obj_set_size(m_mm.root, MINI_W, MINI_H);
    lv_obj_set_pos(m_mm.root, MINI_W + MINI_GAP, 0);

    /* Canvas the big clock is rendered into so it can actually be SCALED.
     * lv_canvas derives from lv_img, so lv_img_set_zoom works on it — LVGL 8
     * cannot transform text, but it can transform an image of text. */
    zoom_buf = (lv_color_t *)ps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(ZOOM_W, ZOOM_H));
    if (zoom_buf) {
        zoom_canvas = lv_canvas_create(root);
        decor(zoom_canvas);
        lv_canvas_set_buffer(zoom_canvas, zoom_buf, ZOOM_W, ZOOM_H, LV_IMG_CF_TRUE_COLOR);
        lv_img_set_antialias(zoom_canvas, true);
        lv_img_set_pivot(zoom_canvas, 0, 0);      /* shrink toward its top-left */
        lv_obj_add_flag(zoom_canvas, LV_OBJ_FLAG_HIDDEN);
    }

    /* Tap overlay — reuses the smaller weather size, no new type size. */
    lbl_info = lv_label_create(root);
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_info, COL_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(lbl_info, "");
    lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_flag(lbl_info, LV_OBJ_FLAG_HIDDEN);

    hold_bar = lv_obj_create(root);
    decor(hold_bar);
    lv_obj_set_size(hold_bar, 2, 4);
    lv_obj_set_style_bg_color(hold_bar, COL_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hold_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(hold_bar, 2, LV_PART_MAIN);
    lv_obj_align(hold_bar, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_add_flag(hold_bar, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_screen(void) { return scr_clock; }

void ui_set_time(int hour, int minute, bool animate)
{
    Settings &s = settings_get();
    int disp = hour;

    if (!s.use_24h) {
        disp = hour % 12;
        if (disp == 0) disp = 12;
        lv_label_set_text(lbl_ampm, hour < 12 ? "AM" : "PM");
        lv_obj_clear_flag(lbl_ampm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(lbl_ampm);
    } else {
        lv_obj_add_flag(lbl_ampm, LV_OBJ_FLAG_HIDDEN);
    }

    char hh[4], mm[4];
    snprintf(hh, sizeof(hh), "%02d", disp);
    snprintf(mm, sizeof(mm), "%02d", minute);

    lv_label_set_text(m_hh.label, hh);
    lv_label_set_text(m_mm.label, mm);

    if (animate) {
        /* Driven by "rendered digits differ from target", never by
         * arithmetic on the previous value — that is what makes
         * 23:59 -> 00:00 and the first post-NTP jump behave. */
        flip_card(card_h, hh);
        flip_card(card_m, mm);
    } else {
        strncpy(card_h.text, hh, sizeof(card_h.text));
        strncpy(card_m.text, mm, sizeof(card_m.text));
        lv_label_set_text(card_h.label, hh);
        lv_label_set_text(card_m.label, mm);
    }
}

void ui_set_weather(float current, float lo, float hi, float humidity,
                    bool valid, bool stale)
{
    if (valid) {
        char t1[16], t2[24], t3[12];
        snprintf(t1, sizeof(t1), "%d" WEATHER_UNIT_SUFFIX, (int)lroundf(current));
        snprintf(t2, sizeof(t2), "%d" WEATHER_UNIT_SUFFIX "/%d" WEATHER_UNIT_SUFFIX,
                 (int)lroundf(lo), (int)lroundf(hi));
        /* Negative means the endpoint did not report it — show nothing rather
         * than a confident 0%. */
        if (humidity >= 0.0f) snprintf(t3, sizeof(t3), "%d%%", (int)lroundf(humidity));
        else                  snprintf(t3, sizeof(t3), "--%%");

        lv_label_set_text(w_temp.label,   t1);
        lv_label_set_text(w_minmax.label, t2);
        lv_label_set_text(w_hum.label,    t3);
        weather_sync();

    }
    if (stale) lv_obj_clear_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_humidity(bool visible)
{
    if (visible) lv_obj_clear_flag(w_hum.root, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(w_hum.root, LV_OBJ_FLAG_HIDDEN);
    weather_sync();
}

void ui_show_weather_block(bool visible)
{
    if (visible) lv_obj_clear_flag(weather_grp, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(weather_grp, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_offset(int dx, int dy)
{
    lv_obj_set_pos(root, dx, dy);
}

void ui_show_info(const char *date_line, const char *sync_line)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s\n%s", date_line, sync_line);
    lv_label_set_text(lbl_info, buf);
    lv_obj_clear_flag(lbl_info, LV_OBJ_FLAG_HIDDEN);
    if (info_timer) { lv_timer_del(info_timer); }
    info_timer = lv_timer_create(hide_info_cb, 5000, nullptr);
    lv_timer_set_repeat_count(info_timer, 1);
}

/* ====================================================================== */
/*  Emotion overlay — one continuous neon line                            */
/* ====================================================================== */
/*
 * Design history, because the first three attempts all failed differently:
 *   v1  3 px accent on the seam        -> invisible against 150 px digits
 *   v2  eyes                           -> pale blobs colliding with numerals
 *   v3  30 segments with gaps          -> read as a DASHED line, not a line
 *   v4  (this) one continuous polyline, and the clock steps aside for it
 *
 * The line is driven by Russell's circumplex (see emotion.h): AROUSAL sets
 * how agitated it is, VALENCE sets its hue. There is no per-emotion
 * animation code — 26 emotions, one renderer.
 */

#define WAVE_PTS     61                      /* 60 segments */
#define WAVE_BAND_H  160
#define COL_CAPTION  lv_color_hex(0xC8C8C8)

#define WEATHER_REST_Y WX_Y

static lv_obj_t   *wave, *lbl_emotion;
static lv_timer_t *wave_timer;
static uint8_t     emo_state;
static float       wave_phase, wave_gain;
static bool        wave_out;

/*
 * The two halves of the transition are STRICTLY SEQUENTIAL, never overlapped:
 *
 *   in    scale the clock into the corner  ->  THEN the line appears
 *   out   the line disappears              ->  THEN scale the clock back
 *
 * Overlapping them reads as two unrelated things happening at once. In
 * sequence it reads as one movement: the clock gets out of the way, and the
 * line takes the space it vacated.
 */
static lv_timer_t *enter_timer;          /* fires once, when the scale lands */
static bool        layout_small;         /* is the clock currently in the corner? */
static char        pending_msg[EMOTION_MSG_MAX + 1];

/* wave_tick() triggers the fly-back, so it needs this before the definition. */
static void layout_emotion(bool on);

/* ------------------------------------------------------- animation glue -- */

static void a_opa (void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, LV_PART_MAIN); }
static void a_zoom(void *o, int32_t v) { lv_img_set_zoom((lv_obj_t *)o, (uint16_t)v); }
static void a_x  (void *o, int32_t v) { lv_obj_set_x((lv_obj_t *)o, v); }
static void a_y  (void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }

static void animate_done(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to,
                         uint32_t ms, uint32_t delay, lv_anim_ready_cb_t ready)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&a, ready);
    lv_anim_start(&a);
}

static void animate(lv_obj_t *obj, lv_anim_exec_xcb_t cb,
                    int32_t from, int32_t to, uint32_t ms, uint32_t delay)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* ------------------------------------------------------------ the line -- */

/*
 * The line's shape.
 *
 * A single sine is periodic, symmetric and instantly reads as a GRAPH. Speech
 * does not look like that. Three ingredients make it read as a voice instead:
 *
 *  1. ADDITIVE HARMONICS at incommensurate ratios (1 : 2.27 : 4.13). Because
 *     the ratios are irrational the sum never repeats on screen, so the motion
 *     looks organic rather than looped.
 *
 *  2. A SPEECH ENVELOPE over time — two slow oscillators at unrelated rates
 *     multiplied together, producing bursts and pauses the way talking does,
 *     instead of a constant-amplitude drone.
 *
 *  3. A TAPER across x, so the line fades to nothing at both edges. This is
 *     the single biggest cue: a stroke that runs edge to edge is a chart, one
 *     that swells in the middle and dies at the ends is a voice.
 */
static float wave_shape(float u, float t, const EmotionDef &d)
{
    /* Spread widened so two processes never look alike: a calm 'waiting' is
     * nearly a flat line, a hot 'flashing' is a full-height scribble. */
    const float k  = 2.6f + d.arousal * 9.0f;      /* spatial frequency  */
    const float sp = 0.45f + d.arousal * 3.1f;     /* temporal speed     */

    float s = 0.55f * sinf(u * k          + t * sp)
            + 0.30f * sinf(u * k * 2.27f  - t * sp * 1.37f + 1.7f)
            + 0.18f * sinf(u * k * 4.13f  + t * sp * 0.71f + 3.1f);

    switch (d.character) {
    case CH_JAGGED:                       /* angular: hard, clipped peaks   */
        s = s > 0.0f ? powf(s, 0.45f) : -powf(-s, 0.45f);
        s += 0.25f * sinf(u * k * 8.0f + t * sp * 2.5f);
        break;
    case CH_TREMOR:                       /* fine judder over the carrier   */
        s = s * 0.7f + 0.4f * sinf(u * k * 9.3f + t * sp * 3.1f);
        break;
    case CH_SCAN: {                       /* a swell travelling along it    */
        float pos = fmodf(t * 0.11f, 1.7f) - 0.35f;
        float dd  = fabsf(u - pos) * 3.6f;
        float env = dd >= 1.0f ? 0.0f : (1.0f - dd) * (1.0f - dd);
        s *= 0.16f + env * 1.4f;
        break;
    }
    case CH_DROOP:                        /* sags below the axis            */
        if (s > 0.0f) s *= 0.22f;
        break;
    default:
        break;
    }
    return s;
}

/* Bursts and pauses, like speech. Two unrelated rates multiplied so the
 * pattern never settles into an obvious cycle. Calm states barely modulate;
 * activated ones burst hard. */
static float wave_speech(float t, float arousal)
{
    float burst = fabsf(sinf(t * 0.85f)) * (0.45f + 0.55f * sinf(t * 0.41f + 2.3f));
    if (burst < 0.0f) burst = -burst;
    float amt = 0.25f + 0.70f * arousal;
    return (1.0f - amt) + amt * (0.30f + 0.70f * burst);
}

static void wave_draw_cb(lv_event_t *e)
{
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    if (wave_gain <= 0.01f) return;

    lv_area_t co;
    lv_obj_get_coords(obj, &co);

    const EmotionDef &d = emotion_def(emo_state);
    const int   width = lv_area_get_width(&co);
    const int   cy    = co.y1 + lv_area_get_height(&co) / 2;
    /* powf pushes the low end down and the high end up, so the amplitude
     * difference between processes reads at a glance rather than needing a
     * side-by-side comparison. 0.15 -> 11 px, 0.65 -> 40 px, 0.95 -> 61 px. */
    const float amp   = (5.0f + powf(d.arousal, 1.25f) * 60.0f) * wave_gain
                      * wave_speech(wave_phase, d.arousal);

    lv_point_t pts[WAVE_PTS];

    for (int i = 0; i < WAVE_PTS; i++) {
        float u = (float)i / (WAVE_PTS - 1);
        /* Taper: sin(pi*u) is exactly zero at both ends, so the stroke dies
         * into the black instead of hitting the bezel. */
        float taper = powf(sinf(u * 3.14159f), 0.75f);
        pts[i].x = co.x1 + (lv_coord_t)(u * (width - 1));
        pts[i].y = cy + (lv_coord_t)(amp * taper * wave_shape(u, wave_phase, d));
    }

    /*
     * ONE opaque stroke. Two earlier mistakes are both fixed here:
     *
     * The polyline is drawn as 60 independent segments with round caps. If
     * the stroke is SEMI-TRANSPARENT, the two caps meeting at each joint
     * overlap and their alpha ACCUMULATES — producing a visibly brighter
     * bead at all 60 vertices. That is what read as "silver dots on top of
     * the line", and it was the wide translucent bloom pass doing it.
     *
     * So: no bloom pass, and opacity is always LV_OPA_COVER. Overlapping
     * opaque white on opaque white is just white, so the joints vanish.
     *
     * The fade in/out therefore cannot use opacity either. It mixes the
     * colour toward black instead — visually identical against a black
     * background, and it keeps every pixel opaque so no beading appears
     * during the transition.
     */
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.round_start = 1;
    dsc.round_end   = 1;
    dsc.width       = 3;
    dsc.opa         = LV_OPA_COVER;
    dsc.color       = lv_color_mix(lv_color_white(), lv_color_black(),
                                   (uint8_t)(wave_gain * 255.0f));

    for (int i = 0; i < WAVE_PTS - 1; i++) {
        lv_draw_line(ctx, &dsc, &pts[i], &pts[i + 1]);
    }
}

static void kill_wave(void)
{
    if (wave_timer) { lv_timer_del(wave_timer); wave_timer = nullptr; }
    if (wave)       { lv_obj_del(wave);         wave = nullptr; }
}

static void wave_tick(lv_timer_t *)
{
    const EmotionDef &d = emotion_def(emo_state);

    /* Entrance and exit are driven here rather than by lv_anim so a rapid
     * emotion change can never leave two waves fighting over one gain. */
    if (wave_out) {
        wave_gain -= 0.09f;
        if (wave_gain <= 0.0f) {
            /* The line is gone. ONLY NOW does the clock fly back. Deleting the
             * running timer from inside its own callback is supported — LVGL
             * flags it and skips the post-callback bookkeeping. */
            kill_wave();
            if (lbl_emotion) { lv_obj_del(lbl_emotion); lbl_emotion = nullptr; }
            emo_state = 0;
            layout_emotion(false);
            return;
        }
    } else if (wave_gain < 1.0f) {
        wave_gain += 0.07f;
        if (wave_gain > 1.0f) wave_gain = 1.0f;
    }

    wave_phase += 0.03f + d.arousal * 0.45f;
    if (wave_phase > 6283.0f) wave_phase = 0.0f;
    if (wave) lv_obj_invalidate(wave);
}

/* ------------------------------------------- the clock scales away ------ */
/*
 * The clock does not cross-fade into a smaller clock — it is genuinely SCALED
 * and flown into the corner, and the weather slides to the opposite corner.
 *
 * LVGL 8 cannot transform text, so the big clock is redrawn into an lv_canvas
 * (which derives from lv_img) and that image is zoomed. At the end of the
 * flight the canvas hands over to the real corner cards, which are sized to
 * exactly the same scale factor — so the handover is invisible rather than a
 * pop, and from then on the digits are real glyphs again rather than a
 * downscaled bitmap.
 */
#define TRANSIT_MS 460

static void zoom_render(void)
{
    if (!zoom_canvas) return;
    lv_canvas_fill_bg(zoom_canvas, COL_BG, LV_OPA_COVER);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = COL_CARD;
    r.bg_opa   = LV_OPA_COVER;
    r.radius   = CARD_RADIUS;
    lv_canvas_draw_rect(zoom_canvas, 0, 0, CARD_W, CARD_H, &r);
    lv_canvas_draw_rect(zoom_canvas, CARD_W + CARD_GAP, 0, CARD_W, CARD_H, &r);

    lv_draw_label_dsc_t l;
    lv_draw_label_dsc_init(&l);
    l.font  = &fliqlo_digits;
    l.color = COL_DIGIT;
    l.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(zoom_canvas, 0, DIGIT_TOP, CARD_W, &l, card_h.text);
    lv_canvas_draw_text(zoom_canvas, CARD_W + CARD_GAP, DIGIT_TOP, CARD_W, &l, card_m.text);

    /* The seam belongs to the big clock, so the canvas carries it. It scales
     * with everything else and is sub-pixel by the time the seamless corner
     * cards take over, so the handover stays invisible. */
    lv_draw_rect_dsc_init(&r);
    r.bg_color = COL_BG;
    r.bg_opa   = LV_OPA_COVER;
    lv_canvas_draw_rect(zoom_canvas, 0, CARD_H / 2 - SEAM_H / 2, CARD_W, SEAM_H, &r);
    lv_canvas_draw_rect(zoom_canvas, CARD_W + CARD_GAP, CARD_H / 2 - SEAM_H / 2,
                        CARD_W, SEAM_H, &r);
}

/* Right-align the weather row against the far edge, measured rather than
 * assumed — the row's width changes with the values in it. */
static int weather_shift(void)
{
    lv_obj_update_layout(weather_grp);
    lv_coord_t right = lv_obj_get_x(w_hum.root) + lv_obj_get_width(w_hum.root);
    if (lv_obj_has_flag(w_hum.root, LV_OBJ_FLAG_HIDDEN))
        right = lv_obj_get_x(w_minmax.root) + lv_obj_get_width(w_minmax.root);
    return (int)(scr_w - 14 - right);
}

static void arrive_small(lv_anim_t *)
{
    lv_obj_add_flag(zoom_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(strip_grp, LV_OPA_COVER, LV_PART_MAIN);
}

static void arrive_big(lv_anim_t *)
{
    lv_obj_add_flag(zoom_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(clock_grp, LV_OBJ_FLAG_HIDDEN);
}

static void layout_emotion(bool on)
{
    layout_small = on;
    const int shift = weather_shift();

    if (!zoom_canvas) {          /* no PSRAM for the canvas — degrade to a fade */
        animate(clock_grp, a_opa, on ? LV_OPA_COVER : LV_OPA_TRANSP,
                on ? LV_OPA_TRANSP : LV_OPA_COVER, TRANSIT_MS, 0);
        lv_obj_set_style_opa(strip_grp, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        animate(weather_grp, a_x, on ? 0 : shift, on ? shift : 0, TRANSIT_MS, 0);
        animate(weather_grp, a_y, on ? WX_Y : MINI_Y, on ? MINI_Y : WX_Y, TRANSIT_MS, 0);
        return;
    }

    zoom_render();
    lv_obj_clear_flag(zoom_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(zoom_canvas);

    if (on) {
        lv_obj_set_pos(zoom_canvas, clock_x, clock_y);
        lv_img_set_zoom(zoom_canvas, 256);
        lv_obj_add_flag(clock_grp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(strip_grp, LV_OPA_TRANSP, LV_PART_MAIN);

        animate_done(zoom_canvas, a_zoom, 256, ZOOM_END, TRANSIT_MS, 0, arrive_small);
        animate(zoom_canvas, a_x, clock_x, MINI_X, TRANSIT_MS, 0);
        animate(zoom_canvas, a_y, clock_y, MINI_Y, TRANSIT_MS, 0);

        animate(weather_grp, a_x, 0,    shift,  TRANSIT_MS, 0);
        animate(weather_grp, a_y, WX_Y, MINI_Y, TRANSIT_MS, 0);
    } else {
        lv_obj_set_pos(zoom_canvas, MINI_X, MINI_Y);
        lv_img_set_zoom(zoom_canvas, ZOOM_END);
        lv_obj_set_style_opa(strip_grp, LV_OPA_TRANSP, LV_PART_MAIN);

        animate_done(zoom_canvas, a_zoom, ZOOM_END, 256, TRANSIT_MS, 0, arrive_big);
        animate(zoom_canvas, a_x, MINI_X, clock_x, TRANSIT_MS, 0);
        animate(zoom_canvas, a_y, MINI_Y, clock_y, TRANSIT_MS, 0);

        animate(weather_grp, a_x, shift,  0,    TRANSIT_MS, 0);
        animate(weather_grp, a_y, MINI_Y, WX_Y, TRANSIT_MS, 0);
    }
}

static void wave_start(void)
{
    kill_wave();
    wave_phase = 0.0f;
    wave_gain  = 0.0f;
    wave_out   = false;

    wave = lv_obj_create(root);
    decor(wave);
    lv_obj_set_size(wave, scr_w, WAVE_BAND_H);
    lv_obj_set_pos(wave, 0, scr_h / 2 - WAVE_BAND_H / 2);
    lv_obj_add_event_cb(wave, wave_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    wave_timer = lv_timer_create(wave_tick, 40, nullptr);   /* 25 fps */

    if (pending_msg[0]) {
        if (lbl_emotion) lv_obj_del(lbl_emotion);
        lbl_emotion = lv_label_create(root);
        lv_obj_set_style_text_font(lbl_emotion, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_emotion, COL_CAPTION, LV_PART_MAIN);
        lv_label_set_text(lbl_emotion, pending_msg);
        lv_obj_align(lbl_emotion, LV_ALIGN_BOTTOM_MID, 0, -26);
    }
}

static void enter_done_cb(lv_timer_t *tm)
{
    enter_timer = nullptr;
    lv_timer_del(tm);
    wave_start();          /* the scale has landed — now the line appears */
}

void ui_emotion_clear(void)
{
    if (!layout_small && !wave) return;

    if (enter_timer) { lv_timer_del(enter_timer); enter_timer = nullptr; }

    if (wave) {
        /* Fade the line out first; wave_tick flies the clock back when the
         * gain reaches zero. */
        wave_out = true;
        return;
    }

    /* Cleared mid-scale, before the line ever appeared — just go back. */
    if (lbl_emotion) { lv_obj_del(lbl_emotion); lbl_emotion = nullptr; }
    emo_state = 0;
    layout_emotion(false);
}

void ui_emotion_show(uint8_t state, const char *message)
{
    emo_state = state;
    strncpy(pending_msg, message ? message : "", EMOTION_MSG_MAX);
    pending_msg[EMOTION_MSG_MAX] = '\0';

    if (enter_timer) { lv_timer_del(enter_timer); enter_timer = nullptr; }

    if (wave && !wave_out) {
        /* Already in line mode — swap the caption, keep the line running.
         * Re-entering the whole transition between two activities would be
         * distracting when the states change every few seconds. */
        if (lbl_emotion) { lv_obj_del(lbl_emotion); lbl_emotion = nullptr; }
        if (pending_msg[0]) {
            lbl_emotion = lv_label_create(root);
            lv_obj_set_style_text_font(lbl_emotion, &lv_font_montserrat_28, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_emotion, COL_CAPTION, LV_PART_MAIN);
            lv_label_set_text(lbl_emotion, pending_msg);
            lv_obj_align(lbl_emotion, LV_ALIGN_BOTTOM_MID, 0, -26);
        }
        return;
    }

    wave_out = false;

    if (layout_small) {
        /* Clock is already parked — a new emotion arrived during a fade-out,
         * so there is nothing to scale. Bring the line straight back. */
        wave_start();
    } else {
        layout_emotion(true);
        enter_timer = lv_timer_create(enter_done_cb, TRANSIT_MS + 30, nullptr);
        lv_timer_set_repeat_count(enter_timer, 1);
    }

    const EmotionDef &d = emotion_def(state);
    Serial.printf("[ui] %s  valence=%+.2f arousal=%.2f char=%u\n",
                  d.name, d.valence, d.arousal, d.character);
}
