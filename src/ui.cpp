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
static lv_obj_t *clock_grp, *weather_grp, *lbl_mini;
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
    weather_grp = lv_obj_create(root);
    decor(weather_grp);
    lv_obj_set_size(weather_grp, screen_w, 70);
    lv_obj_set_pos(weather_grp, 0, clock_y + CARD_H + 26);

    wrow = lv_obj_create(weather_grp);
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

    lv_obj_align(wrow, LV_ALIGN_TOP_MID, 0, 0);

    /* Small clock for emotion mode — the 210 px font cannot be scaled, so the
     * corner clock is a different label rather than a transform. */
    lbl_mini = lv_label_create(root);
    lv_obj_set_style_text_font(lbl_mini, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_mini, COL_DIGIT, LV_PART_MAIN);
    lv_obj_set_style_opa(lbl_mini, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_text(lbl_mini, "00:00");
    lv_obj_set_pos(lbl_mini, 24, 14);

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

    {
        char mini[8];
        snprintf(mini, sizeof(mini), "%s:%s", hh, mm);
        lv_label_set_text(lbl_mini, mini);
    }

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

#define WAVE_PTS     49                      /* 48 segments */
#define WAVE_BAND_H  160
#define COL_CAPTION  lv_color_hex(0xC8C8C8)

#define WEATHER_REST_Y (44 + CARD_H + 26)
#define WEATHER_EMO_Y  14
#define WEATHER_EMO_X  150
#define MINI_Y         14

static lv_obj_t   *wave, *lbl_emotion;
static lv_timer_t *wave_timer;
static uint8_t     emo_state;
static float       wave_phase, wave_hue, wave_gain;
static bool        wave_out;

/* ------------------------------------------------------- animation glue -- */

static void a_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, LV_PART_MAIN); }
static void a_x  (void *o, int32_t v) { lv_obj_set_x((lv_obj_t *)o, v); }
static void a_y  (void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }

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

/* Shape of the line at position u (0..1) for this character. */
static float wave_sample(float u, float ph, uint8_t character)
{
    switch (character) {
    case CH_JAGGED: {                    /* triangle: hard, angular, angry  */
        float x = fmodf(ph, 6.2832f) / 3.1416f;   /* 0..2 */
        return (x < 1.0f ? x : 2.0f - x) * 2.0f - 1.0f;
    }
    case CH_TREMOR:                      /* judder riding a slow carrier    */
        return sinf(ph) * 0.55f + sinf(ph * 6.5f) * 0.45f;
    case CH_SCAN: {                      /* a swell travelling along it     */
        float pos = fmodf(wave_phase * 0.09f, 1.6f) - 0.3f;
        float d   = fabsf(u - pos) * 4.5f;
        float env = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
        return sinf(ph) * (0.18f + env);
    }
    case CH_DROOP: {                     /* sags — sad, deflated            */
        float s = sinf(ph);
        return s > 0.0f ? s * 0.22f : s;
    }
    default:
        return sinf(ph);
    }
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
    const float amp   = (6.0f + d.arousal * 40.0f) * wave_gain;
    const float k     = (0.8f + d.arousal * 2.2f) * 6.2832f;   /* cycles */

    /* Valence -> hue. -1 lands on red, 0 on violet-blue, +1 on cyan-green:
     * the synthwave palette read straight off the circumplex x-axis. */
    const float base_hue = 350.0f - (d.valence + 1.0f) * 0.5f * 185.0f;

    lv_point_t pts[WAVE_PTS];
    lv_color_t col[WAVE_PTS];

    for (int i = 0; i < WAVE_PTS; i++) {
        float u  = (float)i / (WAVE_PTS - 1);
        float ph = wave_phase + u * k;
        pts[i].x = co.x1 + (lv_coord_t)(u * (width - 1));
        pts[i].y = cy + (lv_coord_t)(amp * wave_sample(u, ph, d.character));

        float hue = d.spectrum
                  ? fmodf(u * 360.0f + wave_hue, 360.0f)
                  : base_hue + 16.0f * sinf(wave_phase * 0.25f + u * 1.6f);
        if (hue < 0.0f) hue += 360.0f;
        col[i] = lv_color_hsv_to_rgb((uint16_t)hue, 95, 100);
    }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.round_start = 1;
    dsc.round_end   = 1;

    /* Two passes. The wide dim pass is a fake bloom — the panel has no glow,
     * and on true black this is what sells "neon" rather than "coloured
     * line". Round caps on both make the joints seamless, which is the whole
     * difference between a line and the dashes of v3. */
    for (int pass = 0; pass < 2; pass++) {
        dsc.width = pass == 0 ? 17 : 6;
        dsc.opa   = pass == 0 ? (lv_opa_t)(LV_OPA_40 * wave_gain)
                              : (lv_opa_t)(LV_OPA_COVER * wave_gain);
        for (int i = 0; i < WAVE_PTS - 1; i++) {
            dsc.color = col[i];
            lv_draw_line(ctx, &dsc, &pts[i], &pts[i + 1]);
        }
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
        if (wave_gain <= 0.0f) { kill_wave(); return; }
    } else if (wave_gain < 1.0f) {
        wave_gain += 0.07f;
        if (wave_gain > 1.0f) wave_gain = 1.0f;
    }

    wave_phase += 0.03f + d.arousal * 0.45f;
    if (wave_phase > 6283.0f) wave_phase = 0.0f;
    wave_hue += 1.2f + d.arousal * 2.5f;
    if (wave_hue >= 360.0f) wave_hue -= 360.0f;

    if (wave) lv_obj_invalidate(wave);
}

/* ------------------------------------------------- clock steps aside --- */

static void layout_emotion(bool on)
{
    if (on) {
        animate(clock_grp,   a_opa, LV_OPA_COVER, LV_OPA_TRANSP, 380, 0);
        animate(clock_grp,   a_y,   0, -34, 380, 0);
        animate(lbl_mini,    a_opa, LV_OPA_TRANSP, LV_OPA_COVER, 380, 160);
        animate(lbl_mini,    a_y,   MINI_Y - 22, MINI_Y, 380, 160);
        animate(weather_grp, a_y,   WEATHER_REST_Y, WEATHER_EMO_Y, 420, 0);
        animate(weather_grp, a_x,   0, WEATHER_EMO_X, 420, 0);
    } else {
        animate(clock_grp,   a_opa, LV_OPA_TRANSP, LV_OPA_COVER, 380, 120);
        animate(clock_grp,   a_y,   -34, 0, 380, 120);
        animate(lbl_mini,    a_opa, LV_OPA_COVER, LV_OPA_TRANSP, 260, 0);
        animate(lbl_mini,    a_y,   MINI_Y, MINI_Y - 22, 260, 0);
        animate(weather_grp, a_y,   WEATHER_EMO_Y, WEATHER_REST_Y, 420, 60);
        animate(weather_grp, a_x,   WEATHER_EMO_X, 0, 420, 60);
    }
}

void ui_emotion_clear(void)
{
    if (!wave && emo_state == 0) return;
    wave_out = true;                       /* wave_tick tears it down       */
    if (lbl_emotion) { lv_obj_del(lbl_emotion); lbl_emotion = nullptr; }
    layout_emotion(false);
    emo_state = 0;
}

void ui_emotion_show(uint8_t state, const char *message)
{
    bool was_showing = (wave != nullptr);

    /* A fresh emotion replaces the old one outright — no cross-fade, which
     * would mean two waves sharing one gain. */
    kill_wave();
    if (lbl_emotion) { lv_obj_del(lbl_emotion); lbl_emotion = nullptr; }

    emo_state  = state;
    wave_phase = 0.0f;
    wave_hue   = 0.0f;
    wave_gain  = 0.0f;
    wave_out   = false;

    if (!was_showing) layout_emotion(true);

    wave = lv_obj_create(root);
    decor(wave);
    lv_obj_set_size(wave, scr_w, WAVE_BAND_H);
    lv_obj_set_pos(wave, 0, scr_h / 2 - WAVE_BAND_H / 2);
    lv_obj_add_event_cb(wave, wave_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);

    wave_timer = lv_timer_create(wave_tick, 40, nullptr);   /* 25 fps */

    if (message && message[0]) {
        lbl_emotion = lv_label_create(root);
        lv_obj_set_style_text_font(lbl_emotion, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_emotion, COL_CAPTION, LV_PART_MAIN);
        lv_label_set_text(lbl_emotion, message);
        lv_obj_align(lbl_emotion, LV_ALIGN_BOTTOM_MID, 0, -26);
    }

    const EmotionDef &d = emotion_def(state);
    Serial.printf("[ui] %s  valence=%+.2f arousal=%.2f char=%u%s\n",
                  d.name, d.valence, d.arousal, d.character,
                  d.spectrum ? " spectrum" : "");
}
