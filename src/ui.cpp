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
#include "emotion.h"

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
#define SEAM_H          3
#define COLON_DOT       14
#define LABEL_H         154      /* fliqlo_digits line_height */
#define DIGIT_TOP       ((CARD_H - LABEL_H) / 2)

#define FOLD_MS         180      /* per phase; two phases per flip */

/* Gesture thresholds */
#define TAP_MAX_MS      400
#define BRIGHT_MIN_MS   1200
#define SETTINGS_MS     3000     /* hold-to-open-Settings */

struct Card {
    lv_obj_t *root;
    lv_obj_t *label;
    lv_obj_t *seam;
    lv_obj_t *accent;   /* emotion pulse, sits on the seam, hidden at rest */
    char      text[4];
};

static lv_obj_t *scr_clock;
static lv_obj_t *root;
static Card      card_h, card_m;
static lv_obj_t *lbl_ampm;
static lv_obj_t *wrow, *lbl_temp, *lbl_minmax, *lbl_humid, *dot_stale;
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

    /* The accent line lives on the seam and is transparent at rest, so the
     * default clock is exactly as the brief specifies — nothing extra. */
    c.accent = lv_obj_create(c.root);
    decor(c.accent);
    /* 10 px, not 3: at rest it is fully transparent so the clock is exactly
     * as the brief specifies, but when an emotion lights it up it has to be
     * visible across a room, not a hairline. */
    lv_obj_set_size(c.accent, CARD_W, 10);
    lv_obj_set_style_bg_color(c.accent, COL_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c.accent, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(c.accent, LV_ALIGN_CENTER, 0, 0);
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

    lbl_humid = lv_label_create(wrow);
    lv_obj_set_style_text_font(lbl_humid, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_humid, COL_TERTIARY, LV_PART_MAIN);
    /* A little extra breathing room so it reads as a separate, quieter fact
     * rather than as part of the min/max pair. */
    lv_obj_set_style_pad_left(lbl_humid, 14, LV_PART_MAIN);
    lv_label_set_text(lbl_humid, "");

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

void ui_set_weather(float current, float lo, float hi, float humidity,
                    bool valid, bool stale)
{
    if (valid) {
        char buf[28];
        snprintf(buf, sizeof(buf), "%d" WEATHER_UNIT_SUFFIX, (int)lroundf(current));
        lv_label_set_text(lbl_temp, buf);
        snprintf(buf, sizeof(buf), "%d" WEATHER_UNIT_SUFFIX " / %d" WEATHER_UNIT_SUFFIX,
                 (int)lroundf(lo), (int)lroundf(hi));
        lv_label_set_text(lbl_minmax, buf);

        /* Negative means the endpoint did not report it — show nothing
         * rather than a confident 0%. */
        if (humidity >= 0.0f) {
            snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(humidity));
            lv_label_set_text(lbl_humid, buf);
        } else {
            lv_label_set_text(lbl_humid, "");
        }
    }
    if (stale) lv_obj_clear_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(dot_stale, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_humidity(bool visible)
{
    if (visible) lv_obj_clear_flag(lbl_humid, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(lbl_humid, LV_OBJ_FLAG_HIDDEN);
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

/* ====================================================================== */
/*  Emotion overlay — the neon wave                                       */
/* ====================================================================== */
/*
 * Third design, and the first one the owner could actually read.
 *
 *   v1  a 3 px accent line on the seam. Invisible against 150 px digits.
 *   v2  added eyes. They rendered as pale blobs colliding with the numerals
 *       — legible, but ugly, and nothing like the product's aesthetic.
 *   v3  (this) no eyes. An 80s-style neon sine line running through the card
 *       seam: it moves, it shifts hue, and it is unmistakable without
 *       obscuring anything.
 *
 * PERFORMANCE — why this is ONE object with a custom draw callback rather
 * than 30 little ones:
 *
 * Moving 30 separate objects would queue up to 60 invalidated rectangles per
 * frame. LVGL's invalid-area buffer is finite (LV_INV_BUF_SIZE); overflow it
 * and LVGL gives up and redraws the WHOLE screen — 600*450*2 = 540 KB a
 * frame, which at 30 fps is 16 MB/s against a QSPI bus whose theoretical
 * ceiling is ~18 MB/s. It would stutter.
 *
 * Drawing every segment inside a single object's LV_EVENT_DRAW_MAIN gives
 * exactly one invalid region — the 600x96 band, ~115 KB a frame, ~3.4 MB/s.
 * Comfortable.
 */

#define WAVE_SEGS    30
#define WAVE_BAND_H  96
#define COL_CAPTION  lv_color_hex(0xBFBFBF)

struct WaveStyle {
    float    amp;        /* peak deflection, px            */
    float    speed;      /* phase advance per frame        */
    float    k;          /* spatial frequency along the line */
    uint16_t hue0;       /* base hue, degrees              */
    uint16_t hue_span;   /* hue spread across the line     */
    float    hue_drift;  /* hue rotation per frame         */
    bool     jagged;     /* square off the wave (error)    */
    uint8_t  seg_h;
};

/* Indexed by EmotionState. Each state gets a distinct *motion*, because
 * motion reads at a glance far better than colour alone. */
static const WaveStyle WAVE_STYLES[7] = {
    /* none      */ {  0, 0.00f, 0.00f,   0,   0, 0.00f, false, 6 },
    /* thinking  */ { 16, 0.10f, 0.42f, 195,  60, 0.40f, false, 8 },
    /* working   */ { 10, 0.26f, 0.36f, 185,  45, 0.90f, false, 8 },
    /* success   */ { 24, 0.30f, 0.50f, 120,  40, 0.30f, false, 9 },
    /* error     */ { 28, 0.60f, 1.60f,   0,  20, 0.00f, true,  9 },
    /* celebrate */ { 30, 0.34f, 0.46f,   0, 360, 3.00f, false, 9 },
    /* sleepy    */ {  7, 0.05f, 0.30f, 275,  35, 0.15f, false, 6 },
};

static lv_obj_t   *wave;
static lv_timer_t *wave_timer;
static lv_obj_t   *lbl_emotion;
static uint8_t     emo_state;
static float       wave_phase;
static float       wave_hue;

static const WaveStyle &wave_style(void)
{
    return WAVE_STYLES[emo_state <= 6 ? emo_state : 0];
}

static void wave_draw_cb(lv_event_t *e)
{
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    lv_area_t co;
    lv_obj_get_coords(obj, &co);

    const WaveStyle &w = wave_style();
    if (w.amp <= 0.0f) return;

    const int width = lv_area_get_width(&co);
    const int cy    = co.y1 + lv_area_get_height(&co) / 2;
    const int pitch = width / WAVE_SEGS;
    int sw = pitch - 6;
    if (sw < 4) sw = 4;

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;

    for (int i = 0; i < WAVE_SEGS; i++) {
        float ph = wave_phase + i * w.k;
        /* error squares the wave off into hard alternating spikes; every
         * other state rides a clean sine. */
        float s  = w.jagged ? (((i & 1) ? 1.0f : -1.0f) * fabsf(sinf(ph)))
                            : sinf(ph);
        int   y  = cy + (int)(w.amp * s);

        int hue = (int)(w.hue0 + (i * w.hue_span) / WAVE_SEGS + wave_hue) % 360;
        if (hue < 0) hue += 360;
        lv_color_t c = lv_color_hsv_to_rgb((uint16_t)hue, 90, 100);

        int x = co.x1 + i * pitch + (pitch - sw) / 2;
        int h = w.seg_h;

        /* A dim oversized pass under each segment fakes a neon bloom — the
         * panel has no real glow, and on true black this reads convincingly. */
        dsc.bg_color = c;
        dsc.bg_opa   = LV_OPA_30;
        lv_area_t glow = { (lv_coord_t)(x - 5), (lv_coord_t)(y - h / 2 - 5),
                           (lv_coord_t)(x + sw + 5), (lv_coord_t)(y + h / 2 + 5) };
        lv_draw_rect(ctx, &dsc, &glow);

        dsc.bg_opa = LV_OPA_COVER;
        lv_area_t core = { (lv_coord_t)x, (lv_coord_t)(y - h / 2),
                           (lv_coord_t)(x + sw), (lv_coord_t)(y + h / 2) };
        lv_draw_rect(ctx, &dsc, &core);
    }
}

static void wave_tick(lv_timer_t *)
{
    const WaveStyle &w = wave_style();
    wave_phase += w.speed;
    if (wave_phase > 6283.0f) wave_phase = 0.0f;     /* keep sinf() happy */
    wave_hue += w.hue_drift;
    if (wave_hue >= 360.0f) wave_hue -= 360.0f;
    if (wave) lv_obj_invalidate(wave);
}

/* Push the clock back so the wave is what the eye lands on. The digits stay
 * legible — this is a change of emphasis, not a takeover. */
static void recede_clock(bool on)
{
    Card *cards[2] = { &card_h, &card_m };
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_text_opa(cards[i]->label,
                                  on ? LV_OPA_30 : LV_OPA_COVER, LV_PART_MAIN);
    }
}

void ui_emotion_clear(void)
{
    if (wave_timer)  { lv_timer_del(wave_timer);  wave_timer = nullptr; }
    if (wave)        { lv_obj_del(wave);          wave = nullptr; }
    if (lbl_emotion) { lv_obj_del(lbl_emotion);   lbl_emotion = nullptr; }
    recede_clock(false);
    emo_state = 0;
}

void ui_emotion_show(uint8_t state, const char *message)
{
    ui_emotion_clear();
    emo_state  = state;
    wave_phase = 0.0f;
    wave_hue   = 0.0f;

    recede_clock(true);

    /* Centred on the card seam — the seam is already this design's line, so
     * the wave reads as that line coming alive rather than as a new element
     * bolted on. */
    wave = lv_obj_create(root);
    decor(wave);
    lv_obj_set_size(wave, scr_w, WAVE_BAND_H);
    lv_obj_set_pos(wave, 0, clock_y + CARD_H / 2 - WAVE_BAND_H / 2);
    lv_obj_add_event_cb(wave, wave_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);

    wave_timer = lv_timer_create(wave_tick, 33, nullptr);   /* ~30 fps */

    if (message && message[0]) {
        lbl_emotion = lv_label_create(root);
        lv_obj_set_style_text_font(lbl_emotion, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_emotion, COL_CAPTION, LV_PART_MAIN);
        lv_label_set_text(lbl_emotion, message);
        lv_obj_align(lbl_emotion, LV_ALIGN_BOTTOM_MID, 0, -26);
    }

    const WaveStyle &w = wave_style();
    Serial.printf("[ui] emotion %u: wave amp=%.0f speed=%.2f hue=%u+%u drift=%.1f%s\n",
                  state, w.amp, w.speed, w.hue0, w.hue_span, w.hue_drift,
                  w.jagged ? " jagged" : "");
}
