/**
 * app_themes.cpp — pick the device's look. See D049.
 *
 * One card per theme: its name plus a live swatch strip built FROM the
 * theme's own colors, so the preview cannot drift from the truth. Applying
 * saves the choice and rides the rotation-live machinery, which already
 * knows how to rebuild the clock and re-open this app — so the new look is
 * standing everywhere before the finger lifts.
 */
#include "app_api.h"
#include "app_host.h"
#include "app.h"
#include "theme.h"
#include "settings.h"

#include <Arduino.h>
#include <stdio.h>

#define COL_BG    lv_color_hex(0x000000)
#define COL_TEXT  lv_color_hex(0xE8E8E8)
#define COL_DIM   lv_color_hex(0x8A8A8A)
#define COL_OK    lv_color_hex(0x2FBF71)

static lv_obj_t *scr;

static void apply_cb(lv_event_t *e)
{
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    Settings &s = settings_get();
    if (s.theme == (uint8_t)i) return;
    s.theme = (uint8_t)i;
    settings_save();
    /* Rebuild everything visible in the new colors, staying in this app. */
    app_apply_rotation_live(s.rotation);
}

static void theme_card(lv_obj_t *parent, int i, int x, int y, int w, int h)
{
    const Theme &t = theme_at(i);
    const bool current = (settings_get().theme == (uint8_t)i);

    lv_obj_t *card = lv_btn_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x121212), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, current ? 3 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COL_OK, LV_PART_MAIN);
    lv_obj_add_event_cb(card, apply_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *nm = lv_label_create(card);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(nm, COL_TEXT, LV_PART_MAIN);
    lv_label_set_text(nm, t.name);
    lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 14, 10);

    if (current) {
        lv_obj_t *cur = lv_label_create(card);
        lv_obj_set_style_text_font(cur, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(cur, COL_OK, LV_PART_MAIN);
        lv_label_set_text(cur, LV_SYMBOL_OK "  current");
        lv_obj_align(cur, LV_ALIGN_TOP_RIGHT, -14, 14);
    }

    /* The swatch strip: two clock cards, colon, two weather cards — the
     * actual layout at 1:6 scale, in the theme's actual colors. */
    const uint32_t sw[4] = { t.card, t.card2, t.wx_temp, t.wx_hum };
    const int sw_w = (w - 28 - 3 * 8) / 4;
    for (int k = 0; k < 4; k++) {
        lv_obj_t *s = lv_obj_create(card);
        lv_obj_remove_style_all(s);
        lv_obj_set_size(s, sw_w, h - 64);
        lv_obj_set_pos(s, 14 + k * (sw_w + 8), 48);
        lv_obj_set_style_bg_color(s, lv_color_hex(sw[k]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(s, 10 + t.radius_add, LV_PART_MAIN);
        lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
        if (k < 2) {
            lv_obj_t *d = lv_label_create(s);
            lv_obj_set_style_text_font(d, &lv_font_montserrat_28, LV_PART_MAIN);
            lv_obj_set_style_text_color(d, lv_color_hex(t.digit), LV_PART_MAIN);
            lv_label_set_text(d, k ? "34" : "12");
            lv_obj_center(d);
        }
    }
}

static void themes_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    /* Four little tiles — the bento board in miniature. */
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = 8;
    const uint32_t c[4] = { 0x3D8BE8, 0xF08A28, 0xE8442C, 0xF5D22D };
    for (int i = 0; i < 4; i++) {
        r.bg_color = lv_color_hex(c[i]);
        r.bg_opa   = LV_OPA_COVER;
        lv_area_t a = { (lv_coord_t)(cx - 34 + (i % 2) * 38),
                        (lv_coord_t)(cy - 34 + (i / 2) * 38),
                        (lv_coord_t)(cx - 4  + (i % 2) * 38),
                        (lv_coord_t)(cy - 4  + (i / 2) * 38) };
        lv_draw_rect(ctx, &r, &a);
    }
}

static lv_obj_t *themes_create(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);
    const bool portrait = H > W;

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(t, "Themes   ·   tap to apply, everything rebuilds");
    lv_obj_set_pos(t, 14, 12);

    const int n = theme_count();
    if (portrait) {
        const int ch = (H - 44 - 76 - (n - 1) * 12 - 10) / n;
        for (int i = 0; i < n; i++)
            theme_card(scr, i, 14, 44 + i * (ch + 12), W - 28, ch);
    } else {
        const int cw = (W - 28 - (n - 1) * 12) / n;
        for (int i = 0; i < n; i++)
            theme_card(scr, i, 14 + i * (cw + 12), 44, cw, H - 44 - 76 - 10);
    }

    app_host_std_back(scr, nullptr);
    return scr;
}

static void themes_destroy(void) { scr = nullptr; }

extern const App app_themes = { "Themes", themes_icon, themes_create, themes_destroy,
                                nullptr, nullptr, /*portrait_ok=*/true };
