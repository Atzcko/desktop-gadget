/**
 * ui.cpp — the Fliqlo layout.
 *
 * Aesthetic rules, from the brief:
 *   - background is pure #000000 so AMOLED pixels are genuinely off
 *   - two rounded dark-charcoal cards with a horizontal centre seam
 *   - huge white digits, 24h HH : MM
 *   - nothing else on screen except the weather block
 *   - at most two type sizes besides the clock digits
 */
#include "ui.h"
#include "config.h"
#include <stdio.h>
#include <math.h>

/* fliqlo_digits.c is compiled as C. Without the extern "C" wrapper the
 * C++ compiler would mangle the symbol and the link would fail. */
extern "C" {
LV_FONT_DECLARE(fliqlo_digits);
}

/* ------------------------------------------------------------- palette -- */
#define COL_BG          lv_color_hex(0x000000)  /* true black: pixels off */
#define COL_CARD        lv_color_hex(0x161616)  /* dark charcoal          */
#define COL_DIGIT       lv_color_hex(0xFFFFFF)
#define COL_COLON       lv_color_hex(0x707070)
#define COL_TEMP        lv_color_hex(0xFFFFFF)
#define COL_TEMP_MINMAX lv_color_hex(0x8A8A8A)

/* -------------------------------------------------------------- layout -- */
/* Derived from the generated font: each digit advances 1869/16 = 116.8 px,
 * so a two-digit card needs ~234 px of glyph width. 268 leaves ~17 px of
 * padding per side. Card metrics are even numbers so the Stage 5 burn-in
 * walk and LVGL's even-coordinate rounder never fight. */
#define CARD_W          268
#define CARD_H          232
#define CARD_GAP        36
#define CARD_RADIUS     26
#define SEAM_H          3
#define COLON_DOT       14

static lv_obj_t *root;
static lv_obj_t *lbl_hour;
static lv_obj_t *lbl_min;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_minmax;

/* Build one flip card. The seam is created last so it draws *over* the
 * digits — in Fliqlo the split line crosses the numerals, it does not
 * sit behind them. */
static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, lv_obj_t **out_label)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    /* Clip the digits to the rounded card so a future fold animation
     * cannot spill past the card edge. */
    lv_obj_set_style_clip_corner(card, true, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(card);
    lv_obj_set_style_text_font(lbl, &fliqlo_digits, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COL_DIGIT, LV_PART_MAIN);
    lv_label_set_text(lbl, "00");
    lv_obj_center(lbl);
    *out_label = lbl;

    lv_obj_t *seam = lv_obj_create(card);
    lv_obj_remove_style_all(seam);
    lv_obj_set_size(seam, CARD_W, SEAM_H);
    lv_obj_set_style_bg_color(seam, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seam, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(seam, LV_ALIGN_CENTER, 0, 0);

    return card;
}

static void make_colon(lv_obj_t *parent, int cx, int card_y)
{
    for (int i = 0; i < 2; i++) {
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, COLON_DOT, COLON_DOT);
        lv_obj_set_style_bg_color(dot, COL_COLON, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(dot, COLON_DOT / 2, LV_PART_MAIN);
        /* Dots at 1/3 and 2/3 of the card height, straddling the seam. */
        int dy = (i == 0) ? CARD_H / 3 : (CARD_H * 2) / 3;
        lv_obj_set_pos(dot, cx - COLON_DOT / 2, card_y + dy - COLON_DOT / 2);
    }
}

void ui_init(uint16_t screen_w, uint16_t screen_h)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Everything hangs off one root so the Stage 5 burn-in walk can move
     * the entire layout by moving a single object. */
    root = lv_obj_create(scr);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, screen_w, screen_h);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    const int clock_w = CARD_W * 2 + CARD_GAP;
    const int clock_x = (screen_w - clock_w) / 2;
    const int clock_y = 44;

    make_card(root, clock_x, clock_y, &lbl_hour);
    make_card(root, clock_x + CARD_W + CARD_GAP, clock_y, &lbl_min);
    make_colon(root, clock_x + CARD_W + CARD_GAP / 2, clock_y);

    /* Weather block: current temp is the largest element, min/max beside
     * it in the one smaller size. Bottoms aligned, not baselines — close
     * enough at these sizes and it avoids a hand-tuned offset. */
    lv_obj_t *wrow = lv_obj_create(root);
    lv_obj_remove_style_all(wrow);
    lv_obj_set_size(wrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wrow, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wrow, 20, LV_PART_MAIN);
    lv_obj_clear_flag(wrow, LV_OBJ_FLAG_SCROLLABLE);

    lbl_temp = lv_label_create(wrow);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_temp, COL_TEMP, LV_PART_MAIN);
    lv_label_set_text(lbl_temp, "--" WEATHER_UNIT_SUFFIX);

    lbl_minmax = lv_label_create(wrow);
    lv_obj_set_style_text_font(lbl_minmax, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_minmax, COL_TEMP_MINMAX, LV_PART_MAIN);
    lv_label_set_text(lbl_minmax, "-- / --");

    lv_obj_align(wrow, LV_ALIGN_TOP_MID, 0, clock_y + CARD_H + 26);
}

void ui_set_time(int hour, int minute)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", hour);
    lv_label_set_text(lbl_hour, buf);
    snprintf(buf, sizeof(buf), "%02d", minute);
    lv_label_set_text(lbl_min, buf);
}

void ui_set_weather(float current, float lo, float hi, bool valid)
{
    if (!valid) return;   /* keep last good values; Stage 3 adds the stale dot */

    char buf[24];
    snprintf(buf, sizeof(buf), "%d" WEATHER_UNIT_SUFFIX, (int)lroundf(current));
    lv_label_set_text(lbl_temp, buf);

    snprintf(buf, sizeof(buf), "%d" WEATHER_UNIT_SUFFIX " / %d" WEATHER_UNIT_SUFFIX,
             (int)lroundf(lo), (int)lroundf(hi));
    lv_label_set_text(lbl_minmax, buf);
}
