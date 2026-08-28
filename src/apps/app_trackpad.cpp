/**
 * app_trackpad.cpp — the clock as a Bluetooth trackpad. See D051.
 *
 * One finger moves the cursor; a short still tap clicks; a TWO-finger tap
 * right-clicks; a two-finger drag scrolls. Transport is the BLE HID mouse
 * report the device now carries in both identities — pair once in
 * Settings > BLE and the cursor is system-wide, no companion involved.
 *
 * Finger ONE rides the normal LVGL press events. The second finger is the
 * one thing LVGL cannot see, so app_touch_count() peeks at the controller's
 * raw point count each tick while a press is live.
 */
#include "app_api.h"
#include "app_host.h"
#include "app.h"
#include "ble.h"
#include "theme.h"

#include <Arduino.h>
#include <stdio.h>
#include <math.h>

#define COL_BG    lv_color_hex(0x000000)
#define COL_TEXT  lv_color_hex(0xE8E8E8)
#define COL_DIM   lv_color_hex(0x8A8A8A)
#define COL_ERR   lv_color_hex(0xE0483B)

#define TAP_MS        250
#define TAP_SLOP_PX    12
#define GAIN          1.5f
#define SCROLL_DIV      8

static lv_obj_t *scr, *pad, *lbl_state;
static bool      down, two_finger, moved;
static uint32_t  down_ms;
static lv_point_t last_pt, down_pt;
static float     acc_x, acc_y;
static int       scroll_acc;
static uint32_t  click_release_at;   /* pending button-up, ms timestamp */
static uint8_t   click_buttons;

static void state_paint(void)
{
    if (!lbl_state) return;
    if (ble_is_connected()) {
        lv_label_set_text(lbl_state, "connected  ·  1 finger moves, tap clicks,"
                                     "\n2-finger tap right-clicks, 2-finger drag scrolls");
        lv_obj_set_style_text_color(lbl_state, COL_DIM, LV_PART_MAIN);
    } else {
        lv_label_set_text(lbl_state, "no host connected -\npair this device in"
                                     " your computer's Bluetooth settings");
        lv_obj_set_style_text_color(lbl_state, COL_ERR, LV_PART_MAIN);
    }
}

static void pad_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        down = true; two_finger = false; moved = false;
        down_ms = millis();
        down_pt = last_pt = p;
        acc_x = acc_y = 0; scroll_acc = 0;
    } else if (code == LV_EVENT_PRESSING && down) {
        if (app_touch_count() >= 2) two_finger = true;

        const int rdx = p.x - last_pt.x, rdy = p.y - last_pt.y;
        last_pt = p;
        if (abs(p.x - down_pt.x) > TAP_SLOP_PX || abs(p.y - down_pt.y) > TAP_SLOP_PX)
            moved = true;

        if (two_finger) {
            /* two-finger drag = wheel; vertical only, like a laptop pad */
            scroll_acc += rdy;
            const int notches = scroll_acc / SCROLL_DIV;
            if (notches != 0) {
                scroll_acc -= notches * SCROLL_DIV;
                int w = -notches;               /* content follows fingers */
                if (w > 15) w = 15; if (w < -15) w = -15;
                ble_mouse(0, 0, 0, (int8_t)w);
            }
        } else if (moved) {
            acc_x += rdx * GAIN;
            acc_y += rdy * GAIN;
            const int mx = (int)acc_x, my = (int)acc_y;
            if (mx || my) {
                acc_x -= mx; acc_y -= my;
                int cx = mx, cy = my;
                if (cx > 120) cx = 120; if (cx < -120) cx = -120;
                if (cy > 120) cy = 120; if (cy < -120) cy = -120;
                ble_mouse(0, (int8_t)cx, (int8_t)cy, 0);
            }
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        const bool tap = down && !moved && (millis() - down_ms) < TAP_MS;
        down = false;
        if (tap) {
            click_buttons = two_finger ? 0x02 : 0x01;   /* right : left */
            if (ble_mouse(click_buttons, 0, 0, 0))
                click_release_at = millis() + 30;       /* button-up shortly */
        }
    }
}

static void tp_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_TRANSP;
    r.border_color = lv_color_hex(0xE8E8E8);
    r.border_width = 5;
    r.radius = 10;
    lv_area_t a = { (lv_coord_t)(cx - 34), (lv_coord_t)(cy - 26),
                    (lv_coord_t)(cx + 34), (lv_coord_t)(cy + 26) };
    lv_draw_rect(ctx, &r, &a);
    /* the button bar */
    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(0xE8E8E8);
    l.width = 5;
    lv_point_t p1 = { (lv_coord_t)(cx - 30), (lv_coord_t)(cy + 10) };
    lv_point_t p2 = { (lv_coord_t)(cx + 30), (lv_coord_t)(cy + 10) };
    lv_draw_line(ctx, &l, &p1, &p2);
}

static lv_obj_t *tp_create(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* The pad: everything above the strip. A visible inset border says
     * "this surface is the control", the way a laptop pad is felt edges. */
    pad = lv_obj_create(scr);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, W - 16, H - 76 - 14);
    lv_obj_align(pad, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x0E0E0E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(pad, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(pad, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(pad, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(pad, pad_cb, LV_EVENT_ALL, nullptr);

    lbl_state = lv_label_create(pad);
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_state, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(lbl_state, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_state);
    state_paint();

    /* The strip holds only the standard back chip now — the owner dropped
     * the Left/Right buttons; taps on the pad are the clicks. The freed
     * strip width goes to the pad. */
    app_host_std_back(scr, nullptr);

    down = false; click_release_at = 0;
    return scr;
}

static void tp_destroy(void)
{
    /* Never leave a button held on the host. */
    if (click_release_at || down) ble_mouse(0, 0, 0, 0);
    scr = pad = lbl_state = nullptr;
    down = false; click_release_at = 0;
}

static void tp_tick(void)
{
    if (!scr) return;
    if (click_release_at && millis() >= click_release_at) {
        click_release_at = 0;
        ble_mouse(0, 0, 0, 0);               /* button up */
    }
    static uint32_t last_state = 0;
    if (millis() - last_state > 1000) {      /* connection state, lazily */
        last_state = millis();
        state_paint();
    }
}

extern const App app_trackpad = { "Trackpad", tp_icon, tp_create, tp_destroy,
                                  tp_tick, nullptr, /*portrait_ok=*/true };
