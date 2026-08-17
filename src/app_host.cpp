/**
 * app_host.cpp — drawer, launcher, and the way back.
 *
 * The host owns exactly three things: which app is running, the drawer screen,
 * and the route home. It knows nothing about what any app draws, and no app
 * knows anything about the clock. See D026.
 */
#include "app_host.h"
#include "app_api.h"
#include "ui.h"

#include <Arduino.h>

#define COL_BG      lv_color_hex(0x000000)
#define COL_CARD    lv_color_hex(0x161616)
#define COL_TEXT    lv_color_hex(0xE8E8E8)
#define COL_DIM     lv_color_hex(0x8A8A8A)
#define TILE        118

static lv_obj_t *drawer;
static const App *running;

/* Same rule as the clock screen: LVGL makes every object clickable and part of
 * the scroll chain, and a decorative child that keeps those flags swallows the
 * press meant for its parent. */
static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

bool app_host_is_open(void) { return drawer != nullptr || running != nullptr; }

static void teardown(void)
{
    if (running) { running->destroy(); running = nullptr; }
    if (drawer)  { lv_obj_t *d = drawer; drawer = nullptr; lv_obj_del(d); }
}

void app_host_home(void)
{
    if (!app_host_is_open()) return;
    lv_obj_t *dead_drawer = drawer;
    drawer = nullptr;
    if (running) { running->destroy(); running = nullptr; }
    lv_scr_load(ui_screen());
    if (dead_drawer) lv_obj_del(dead_drawer);
}

static void launch_cb(lv_event_t *e)
{
    const App *app = (const App *)lv_event_get_user_data(e);
    if (!app) return;

    /* Drop the drawer before building the app, so only one extra screen is
     * ever resident — the same discipline that keeps PSRAM flat. */
    lv_obj_t *dead = drawer;
    drawer = nullptr;

    running = app;
    lv_obj_t *scr = app->create();
    lv_scr_load(scr);
    if (dead) lv_obj_del(dead);

    Serial.printf("[apps] launched %s\n", app->name);
}

static void close_cb(lv_event_t *) { app_host_home(); }

void app_host_open_drawer(void)
{
    if (app_host_is_open()) return;

    drawer = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(drawer, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(drawer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(drawer);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(title, "Apps");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 18);

    lv_obj_t *row = lv_obj_create(drawer);
    decor(row);
    lv_obj_set_size(row, 600, TILE + 46);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 22, LV_PART_MAIN);

    for (int i = 0; i < APP_COUNT; i++) {
        const App *app = APPS[i];

        lv_obj_t *cell = lv_obj_create(row);
        decor(cell);
        lv_obj_set_size(cell, TILE, TILE + 40);

        lv_obj_t *tile = lv_obj_create(cell);
        lv_obj_remove_style_all(tile);
        lv_obj_set_size(tile, TILE, TILE);
        lv_obj_align(tile, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(tile, COL_CARD, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(tile, 22, LV_PART_MAIN);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, launch_cb, LV_EVENT_CLICKED, (void *)app);
        if (app->icon) lv_obj_add_event_cb(tile, app->icon, LV_EVENT_DRAW_MAIN, nullptr);

        lv_obj_t *lbl = lv_label_create(cell);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, COL_TEXT, LV_PART_MAIN);
        lv_label_set_text(lbl, app->name);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    lv_obj_t *back = lv_btn_create(drawer);
    lv_obj_set_size(back, 200, 46);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_add_event_cb(back, close_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(back);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(bl, LV_SYMBOL_LEFT "  Clock");
    lv_obj_center(bl);

    lv_scr_load(drawer);
}

void app_host_tick(void)
{
    if (running && running->tick) running->tick();
}
