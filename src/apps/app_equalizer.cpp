/**
 * app_equalizer.cpp — the spectrum analyzer (D058).
 *
 * Thirty-two bars that dance to whatever the Mac is playing. The levels
 * arrive ~30×/s from eq.cpp's worker; this file smooths them again on the
 * LVGL tick (fast rise, slow fall) so motion stays fluid between frames,
 * and drops a peak cap on each bar that falls under gravity — the classic
 * analyzer look. Every color comes from the theme table (D049): the bars
 * fade from the digit color into the card color, the caps are the colon.
 * Bars are computed from the live display, so both shapes work (D045).
 */
#include "app_api.h"
#include "app_host.h"
#include "eq.h"
#include "theme.h"

#include <Arduino.h>

#define COL_BG   lv_color_hex(0x000000)
#define COL_DIM  lv_color_hex(0x8A8A8A)

static lv_obj_t *scr, *status_lbl;
static lv_obj_t *bars[EQ_BANDS], *caps[EQ_BANDS];
static float     cur[EQ_BANDS], peak[EQ_BANDS], peakv[EQ_BANDS];
static int       base_y, max_h;
static uint32_t  seen_rev;

/* five bars of a spectrum, for the drawer tile */
static void eq_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(0xE8E8E8);
    d.bg_opa = LV_OPA_COVER;
    d.radius = 3;
    static const int h[5] = { 18, 34, 46, 26, 38 };
    for (int i = 0; i < 5; i++) {
        lv_area_t a = { (lv_coord_t)(cx - 34 + i * 15), (lv_coord_t)(cy + 24 - h[i]),
                        (lv_coord_t)(cx - 24 + i * 15), (lv_coord_t)(cy + 24) };
        lv_draw_rect(ctx, &d, &a);
    }
}

static lv_obj_t *eq_create(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);
    const Theme &t = theme_get();

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(title, "Equalizer");
    lv_obj_set_pos(title, 14, 10);

    /* geometry from the live display: a title row above, the back chip's
     * strip below, bars fill what is left */
    const int top = 40, bottom = 66, margin = 10, gap = 3;
    max_h  = H - top - bottom;
    base_y = H - bottom;
    const int bar_w = (W - 2 * margin - (EQ_BANDS - 1) * gap) / EQ_BANDS;
    const int span  = EQ_BANDS * bar_w + (EQ_BANDS - 1) * gap;
    const int x0    = (W - span) / 2;

    for (int i = 0; i < EQ_BANDS; i++) {
        const int x = x0 + i * (bar_w + gap);

        lv_obj_t *b = lv_obj_create(scr);
        lv_obj_remove_style_all(b);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(b, bar_w, 2);
        lv_obj_set_pos(b, x, base_y - 2);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_hex(t.digit), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(b, lv_color_hex(t.card2), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, LV_PART_MAIN);
        lv_obj_set_style_radius(b, 3, LV_PART_MAIN);
        bars[i] = b;

        lv_obj_t *c = lv_obj_create(scr);
        lv_obj_remove_style_all(c);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(c, bar_w, 3);
        lv_obj_set_pos(c, x, base_y - 5);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(c, lv_color_hex(t.colon), LV_PART_MAIN);
        lv_obj_set_style_radius(c, 1, LV_PART_MAIN);
        caps[i] = c;

        cur[i] = peak[i] = peakv[i] = 0;
    }

    status_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_lbl, COL_DIM, LV_PART_MAIN);
    lv_obj_set_width(status_lbl, LV_PCT(84));
    lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(status_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(status_lbl, LV_ALIGN_CENTER, 0, -20);
    lv_label_set_text(status_lbl, "connecting to the companion...");

    app_host_std_back(scr, nullptr);

    eq_start();
    seen_rev = 0xFFFFFFFF;
    return scr;
}

static void eq_destroy(void)
{
    eq_stop();
    scr = status_lbl = nullptr;
    for (int i = 0; i < EQ_BANDS; i++) bars[i] = caps[i] = nullptr;
}

static void eq_tick(void)
{
    if (!scr) return;
    const bool live = eq_streaming();

    if (eq_rev() != seen_rev) {
        seen_rev = eq_rev();
        const char *st = eq_status();
        if (!live && st[0]) {
            lv_obj_clear_flag(status_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(status_lbl, st);
        } else if (live) {
            lv_obj_add_flag(status_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    uint8_t lv[EQ_BANDS];
    eq_levels(lv);
    for (int i = 0; i < EQ_BANDS; i++) {
        const float target = live ? (lv[i] / 255.0f) * max_h : 0.0f;
        cur[i] += (target - cur[i]) * (target > cur[i] ? 0.65f : 0.22f);
        int h = (int)cur[i];
        if (h < 2) h = 2;

        if (cur[i] >= peak[i]) { peak[i] = cur[i]; peakv[i] = 0; }
        else {
            peakv[i] += 0.30f;                        /* gravity */
            peak[i]  -= peakv[i];
            if (peak[i] < cur[i]) { peak[i] = cur[i]; peakv[i] = 0; }
        }
        int ph = (int)peak[i];
        if (ph < 2) ph = 2;

        if (h != lv_obj_get_height(bars[i])) {
            lv_obj_set_height(bars[i], h);
            lv_obj_set_y(bars[i], base_y - h);
        }
        const int cy = base_y - ph - 3;
        if (cy != lv_obj_get_y(caps[i])) lv_obj_set_y(caps[i], cy);
    }
}

static bool eq_back(void) { return false; }        /* the host pops the app */

extern const App app_equalizer = { "Equalizer", eq_icon, eq_create, eq_destroy,
                                   eq_tick, eq_back, /*portrait_ok=*/true };
