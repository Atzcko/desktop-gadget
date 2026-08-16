/**
 * ui.cpp — the Fliqlo layout, the split-flap fold, and touch gestures.
 *
 * Aesthetic rules from the brief: pure #000000 background so AMOLED pixels
 * are genuinely off; two rounded charcoal cards with a horizontal centre
 * seam; huge white digits; nothing on screen but the clock and weather.
 */
#include "ui.h"
#include "config.h"
#include "settings.h"
#include "ui_settings.h"
#include "app.h"

#include <Arduino.h>
#include <stdio.h>
#include <math.h>

extern "C" {
LV_FONT_DECLARE(fliqlo_digits);
}

#define COL_BG          lv_color_hex(0x000000)
#define COL_CARD        lv_color_hex(0x161616)
#define COL_DIGIT       lv_color_hex(0xFFFFFF)
#define COL_COLON       lv_color_hex(0x707070)
#define COL_TEMP        lv_color_hex(0xFFFFFF)
#define COL_SECONDARY   lv_color_hex(0x8A8A8A)
#define COL_ACCENT      lv_color_hex(0x3C7DD9)
#define COL_STALE       lv_color_hex(0x8A6A2A)

/* See D012 — every constant derives from the 116.8 px digit advance, and
 * every one of them is even so LVGL's even-coordinate rounder never
 * enlarges a redraw region. */
#define CARD_W          268
#define CARD_H          232
#define CARD_GAP        36
#define CARD_RADIUS     26
#define SEAM_H          3
#define COLON_DOT       14
#define LABEL_H         154      /* fliqlo_digits line_height */
#define DIGIT_TOP       ((CARD_H - LABEL_H) / 2)

#define FOLD_MS         180      /* per phase; two phases per flip */

/* Gesture thresholds */
#define TAP_MAX_MS      400
#define BRIGHT_MIN_MS   1200
#define SETTINGS_MS     5000

struct Card {
    lv_obj_t *root;
    lv_obj_t *label;
    lv_obj_t *seam;
    char      text[4];
};

static lv_obj_t *scr_clock;
static lv_obj_t *root;
static Card      card_h, card_m;
static lv_obj_t *lbl_ampm;
static lv_obj_t *wrow, *lbl_temp, *lbl_minmax, *dot_stale;
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
 * 5-second hold working only in the thin margins around the cards.
 *
 * Strip the flag from anything that is only there to be looked at.
 */
static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
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
 *   phase B — the NEW bottom grows from the seam downward, covering it
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
             * this, a 5 s hold is an invisible affordance nobody finds. */
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
    c.root = lv_obj_create(root);
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

void ui_init(uint16_t screen_w, uint16_t screen_h)
{
    scr_w = screen_w;
    scr_h = screen_h;

    scr_clock = lv_scr_act();
    lv_obj_set_style_bg_color(scr_clock, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_clock, LV_OPA_COVER, LV_PART_MAIN);

    root = lv_obj_create(scr_clock);
    decor(root);
    lv_obj_set_size(root, screen_w, screen_h);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, on_press, LV_EVENT_ALL, nullptr);

    const int clock_w = CARD_W * 2 + CARD_GAP;
    clock_x = (screen_w - clock_w) / 2;
    clock_y = 44;

    make_card(card_h, clock_x, clock_y);
    make_card(card_m, clock_x + CARD_W + CARD_GAP, clock_y);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *dot = lv_obj_create(root);
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
    wrow = lv_obj_create(root);
    decor(wrow);
    lv_obj_set_size(wrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wrow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wrow, 20, LV_PART_MAIN);
    lv_obj_clear_flag(wrow, LV_OBJ_FLAG_SCROLLABLE);

    lbl_temp = lv_label_create(wrow);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_temp, COL_TEMP, LV_PART_MAIN);
    lv_label_set_text(lbl_temp, "--" WEATHER_UNIT_SUFFIX);

    lbl_minmax = lv_label_create(wrow);
    lv_obj_set_style_text_font(lbl_minmax, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_minmax, COL_SECONDARY, LV_PART_MAIN);
    lv_label_set_text(lbl_minmax, "-- / --");

    /* Stale marker is a shape, not a third type size. */
    dot_stale = lv_obj_create(wrow);
    decor(dot_stale);
    lv_obj_set_size(dot_stale, 8, 8);
    lv_obj_set_style_bg_color(dot_stale, COL_STALE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot_stale, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dot_stale, 4, LV_PART_MAIN);
    lv_obj_add_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(wrow, LV_ALIGN_TOP_MID, 0, clock_y + CARD_H + 26);

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

void ui_set_weather(float current, float lo, float hi, bool valid, bool stale)
{
    if (valid) {
        char buf[28];
        snprintf(buf, sizeof(buf), "%d" WEATHER_UNIT_SUFFIX, (int)lroundf(current));
        lv_label_set_text(lbl_temp, buf);
        snprintf(buf, sizeof(buf), "%d" WEATHER_UNIT_SUFFIX " / %d" WEATHER_UNIT_SUFFIX,
                 (int)lroundf(lo), (int)lroundf(hi));
        lv_label_set_text(lbl_minmax, buf);
    }
    if (stale) lv_obj_clear_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_weather_block(bool visible)
{
    if (visible) lv_obj_clear_flag(wrow, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(wrow, LV_OBJ_FLAG_HIDDEN);
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
