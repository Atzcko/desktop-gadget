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
#include "settings.h"

/* The drawer renders in the owner's order, not the registry's. app_order holds
 * registry indices; anything out of range falls back to identity so a corrupt
 * or half-written NVS value can never hide an app. */
static const App *order_at(int slot)
{
    Settings &s = settings_get();
    uint8_t i = (slot >= 0 && slot < APP_COUNT) ? s.app_order[slot] : (uint8_t)slot;
    if (i >= APP_COUNT) i = (uint8_t)slot;
    return APPS[i];
}

#include <Arduino.h>

#define COL_BG      lv_color_hex(0x000000)
#define COL_CARD    lv_color_hex(0x161616)
#define COL_TEXT    lv_color_hex(0xE8E8E8)
#define COL_DIM     lv_color_hex(0x8A8A8A)
#define TILE        118

static lv_obj_t  *drawer;
static const App *running;
/*
 * The host holds the screen create() returned and deletes it on the way home.
 * Leaving that to each app meant timer_destroy() only nulled its pointers and
 * leaked a whole screen per launch — the kind of bug that survives because
 * nothing visibly breaks until PSRAM runs out days later.
 */
static lv_obj_t  *running_scr;

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

void app_host_home(void)
{
    if (!app_host_is_open()) return;

    lv_obj_t *dead_drawer = drawer;
    lv_obj_t *dead_app    = running_scr;
    drawer = nullptr;
    running_scr = nullptr;

    if (running) { running->destroy(); running = nullptr; }

    /* Load the clock BEFORE deleting anything — deleting the active screen is
     * how you get a use-after-free on the next render pass. */
    lv_scr_load(ui_screen());
    if (dead_drawer) lv_obj_del(dead_drawer);
    if (dead_app)    lv_obj_del(dead_app);
}

void app_host_launch(const App *app)
{
    if (!app) return;

    /* Drop the drawer before building the app, so only one extra screen is
     * ever resident — the same discipline that keeps PSRAM flat. */
    lv_obj_t *dead = drawer;
    drawer = nullptr;

    running     = app;
    running_scr = app->create();
    lv_scr_load(running_scr);
    if (dead) lv_obj_del(dead);

    Serial.printf("[apps] launched %s\n", app->name);
}

/*
 * LVGL sends LV_EVENT_CLICKED on release WHATEVER the duration — a long press
 * is CLICKED as well as LONG_PRESSED. So a reorder would launch the app it had
 * just moved. This flag is the whole fix; it is cleared on the next press.
 */
static bool reorder_fired;

static void press_cb(lv_event_t *)   { reorder_fired = false; }

static void launch_cb(lv_event_t *e)
{
    if (reorder_fired) { reorder_fired = false; return; }
    app_host_launch((const App *)lv_event_get_user_data(e));
}

/*
 * Long-press a tile to move it one place left, wrapping. Drag-and-drop would be
 * the obvious gesture and the wrong one here: there is no cursor, the tiles are
 * 118 px, and a mis-drop is indistinguishable from a launch. One deterministic
 * step per press is boring and always does what it says.
 *
 * REBOOT, FIXED: this used to delete the drawer and rebuild it — from INSIDE an
 * event callback on one of its own grandchildren. Deleting the active screen
 * mid-dispatch leaves lv_disp_t.act_scr dangling, and the very next
 * lv_obj_create() walks up to it to invalidate. The device reset every time.
 *
 * Nothing needs deleting. The tiles are flex children, so moving one changes
 * the order — the layout is the model. See D031.
 */
static void reorder_cb(lv_event_t *e)
{
    const App *app = (const App *)lv_event_get_user_data(e);
    Settings &s = settings_get();

    int idx = -1;
    for (int i = 0; i < APP_COUNT; i++) if (order_at(i) == app) { idx = i; break; }
    if (idx < 0) return;

    const int prev = (idx - 1 + APP_COUNT) % APP_COUNT;
    uint8_t tmp = s.app_order[idx];
    s.app_order[idx]  = s.app_order[prev];
    s.app_order[prev] = tmp;
    settings_save();

    /* The tile's parent is the cell; the cell is what the flex row lays out. */
    lv_obj_t *cell = lv_obj_get_parent(lv_event_get_target(e));
    if (cell) lv_obj_move_to_index(cell, prev);

    reorder_fired = true;
    Serial.printf("[apps] moved %s to slot %d\n", app->name, prev);
}

static void close_cb(lv_event_t *) { app_host_back(); }

/* Build and load the drawer screen. No guard — callers decide when it is
 * legal. The drawer is rebuilt on every visit rather than kept resident,
 * same discipline as the apps: nothing stays alive in the background. */
static void build_drawer(void)
{
    drawer = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(drawer, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(drawer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(drawer);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(title, "Apps   ·   hold a tile to move it left");
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
        const App *app = order_at(i);

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
        lv_obj_add_event_cb(tile, press_cb,  LV_EVENT_PRESSED, nullptr);
        lv_obj_add_event_cb(tile, launch_cb, LV_EVENT_CLICKED, (void *)app);
        lv_obj_add_event_cb(tile, reorder_cb, LV_EVENT_LONG_PRESSED, (void *)app);
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

void app_host_open_drawer(void)
{
    if (app_host_is_open()) return;
    build_drawer();
}

/*
 * Back pops ONE level of the stack the user actually walked: an app returns to
 * the drawer, the drawer returns to the clock. See D033 — this replaced the
 * original everything-goes-home semantics of D029.
 *
 * The app still gets first refusal, so Settings closes its text editor instead
 * of leaving — the same contract hook the gesture has always offered.
 */
void app_host_back(void)
{
    if (!app_host_is_open()) return;

    if (running) {
        if (running->back && running->back()) return;

        lv_obj_t *dead = running_scr;
        running_scr = nullptr;
        running->destroy();
        running = nullptr;

        /* Load the drawer BEFORE deleting the app's screen — deleting the
         * active screen is how you get a use-after-free (D031). */
        build_drawer();
        if (dead) lv_obj_del(dead);
    } else {
        app_host_home();
    }
}

/* ------------------------------------------------- the way back, as a -- */
/*
 * Swipe in from the LEFT EDGE to go home.
 *
 * This cannot be an event handler on the screen. LVGL 8 does not bubble events,
 * so any clickable child — a timer card, a tab bar, a list, a roller — eats the
 * press before the screen sees it, and "back" would work everywhere except the
 * places you actually need it. Setting EVENT_BUBBLE on every widget an app ever
 * creates is not a contract anyone can keep.
 *
 * So it is POLLED from the LVGL loop, above the widget tree — which is where a
 * system gesture belongs. Apps get it for free and cannot break it, which is
 * rule 3 of the contract solved by construction instead of by discipline.
 *
 * It mirrors the clock's swipe-up: armed by where it STARTS, judged on release.
 */
#define EDGE_ZONE      44     /* px from the left edge the swipe must start */
#define EDGE_MIN_DX   110     /* px of rightward travel to count            */
#define EDGE_MAX_DY    90     /* px of vertical wander still allowed        */

static bool       edge_down, edge_armed;
static lv_point_t edge_p0, edge_last;

static lv_indev_t *pointer_indev(void)
{
    for (lv_indev_t *i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) return i;
    return nullptr;
}

static void back_gesture_tick(void)
{
    lv_indev_t *indev = pointer_indev();
    if (!indev) return;

    /* proc.state because LVGL 8 has no lv_indev_get_state(). The struct is
     * public and this is the one field read from it. */
    const bool down = (indev->proc.state == LV_INDEV_STATE_PRESSED);

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (down) {
        if (!edge_down) {
            edge_p0    = p;
            /* Only while something is open. On the clock, home is where you
             * already are. */
            edge_armed = app_host_is_open() && p.x <= EDGE_ZONE;
        }
        /* Track the last point seen WHILE PRESSED and judge on that. Reading
         * the point after release trusts the touch driver to leave valid
         * coordinates behind, and not all of them do. */
        edge_last = p;
    } else if (edge_down && edge_armed) {
        edge_armed = false;
        if ((edge_last.x - edge_p0.x) >= EDGE_MIN_DX &&
            abs(edge_last.y - edge_p0.y) <= EDGE_MAX_DY) {
            /* One level, not home — and the app still gets first refusal
             * inside app_host_back(). */
            app_host_back();
            edge_down = false;
            return;                      /* the screen may be gone; stop here */
        }
    }
    edge_down = down;
}

void app_host_tick(void)
{
    back_gesture_tick();
    if (running && running->tick) running->tick();
}
