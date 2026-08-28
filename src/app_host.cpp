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
#include "script.h"
#include "app.h"
#include "theme.h"

/*
 * The drawer renders in the owner's order, not the registry's.
 *
 * app_order holds registry indices, and the registry is no longer fixed — a
 * script app can appear or vanish between visits. A saved order that is not a
 * permutation of 0..n-1 is therefore not corruption, it is just stale, and the
 * only safe reading of it is none: fall back to identity for the whole list
 * rather than trust half of it and hide an app. See D037.
 */
static bool order_valid(void)
{
    const int n = app_count();
    Settings &s = settings_get();
    if (n > (int)sizeof(s.app_order)) return false;

    bool seen[sizeof(s.app_order)] = { false };
    for (int i = 0; i < n; i++) {
        uint8_t v = s.app_order[i];
        if (v >= n || seen[v]) return false;
        seen[v] = true;
    }
    return true;
}

static const App *order_at(int slot)
{
    if (slot < 0 || slot >= app_count()) return nullptr;
    if (!order_valid()) return app_at(slot);
    return app_at(settings_get().app_order[slot]);
}

#include <Arduino.h>
#include <ctype.h>

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

/*
 * Before deleting any screen: cut the input device loose from it.
 *
 * LVGL's obj_del_core() clears act_obj, last_obj and last_pressed when an
 * object dies — but NOT scroll_obj. Delete a screen while the finger is
 * mid-scroll on it and the indev keeps a pointer into freed memory, which
 * lv_indev_scroll_throw_handler() dereferences on the very next read. The
 * result is a reboot a fraction of a second after the screen changed, with
 * nothing on screen to connect it to scrolling.
 *
 * lv_indev_reset(NULL, NULL) sets reset_query on every indev, and
 * indev_proc_reset_query_handler() — which runs before any further
 * processing — nulls scroll_obj along with everything else. See D038.
 */
static void release_input(void) { lv_indev_reset(nullptr, nullptr); }

/* Defined beside app_host_launch, used from home/back as well. */
static void force_landscape_for_app(const App *app);
static void restore_user_rotation(void);

bool app_host_is_open(void) { return drawer != nullptr || running != nullptr; }

/* Script apps share one set of C callbacks, so the runtime needs to ask which
 * app it is being called for. Valid during create/destroy/tick/back. */
const App *app_host_running(void) { return running; }

void app_host_home(void)
{
    if (!app_host_is_open()) return;

    lv_obj_t *dead_drawer = drawer;
    lv_obj_t *dead_app    = running_scr;
    drawer = nullptr;
    running_scr = nullptr;

    if (running) { running->destroy(); running = nullptr; }

    restore_user_rotation();     /* the clock screen is laid out for the
                                  * owner's orientation, not the app's */

    /* Load the clock BEFORE deleting anything — deleting the active screen is
     * how you get a use-after-free on the next render pass. */
    release_input();
    lv_scr_load(ui_screen());
    if (dead_drawer) lv_obj_del(dead_drawer);
    if (dead_app)    lv_obj_del(dead_app);
}

/*
 * Every app screen in this firmware — native and script alike — is laid out
 * for 600x450. The clock and the drawer know portrait; the apps do not, and
 * pretending otherwise would clip every one of them at x=450. So the host
 * rotates the PANEL to the nearest landscape for the duration of an app and
 * restores the owner's orientation on the way out (D043). When a
 * portrait-capable app exists, this is where its flag gets honoured.
 */
static bool app_forced_landscape;

static void force_landscape_for_app(const App *app)
{
    if (app && app->portrait_ok) return;        /* lays itself out; D045 */
    const uint8_t r = settings_get().rotation;
    if (!(r & 1)) return;                       /* already landscape */
    app_panel_rotate(r == 1 ? 0 : 2);           /* nearest: 90->0, 270->180 */
    app_forced_landscape = true;
}

static void restore_user_rotation(void)
{
    if (!app_forced_landscape) return;
    app_forced_landscape = false;
    app_panel_rotate(settings_get().rotation);
}

lv_obj_t *app_host_std_back(lv_obj_t *parent, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, 132, 56);
    lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_style_bg_color(b, lv_color_hex(theme_get().chip), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 14 + theme_get().radius_add, LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb ? cb
                              : [](lv_event_t *) { app_host_back(); },
                        LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(l, LV_SYMBOL_LEFT "  Back");
    lv_obj_center(l);
    return b;
}

void app_host_launch(const App *app)
{
    if (!app) return;

    /* Drop the drawer before building the app, so only one extra screen is
     * ever resident — the same discipline that keeps PSRAM flat. */
    lv_obj_t *dead = drawer;
    drawer = nullptr;

    force_landscape_for_app(app);

    running     = app;
    running_scr = app->create();
    release_input();
    lv_scr_load(running_scr);
    if (dead) lv_obj_del(dead);

    Serial.printf("[apps] launched %s\n", app->name);
}

/*
 * LVGL sends LV_EVENT_CLICKED on release WHATEVER the duration — a long press
 * is CLICKED as well as LONG_PRESSED. So a reorder would launch the app it had
 * just moved. This flag is the whole fix; it is cleared on the next press.
 */
static bool       reorder_fired;
static bool       tile_moved;
static lv_point_t tile_press_pt;

#define TILE_MOVE_SLOP 24      /* px: past this the press was a scroll */

static void press_cb(lv_event_t *)
{
    reorder_fired = false;
    tile_moved    = false;
    lv_indev_t *indev = lv_indev_get_act();
    if (indev) lv_indev_get_point(indev, &tile_press_pt);
}

/* Displacement disqualifies a press — the rule the clock has followed since
 * D027, which the drawer never applied. Without it, dragging the list to
 * reach an app launches whichever app the drag started on. */
static void tile_pressing_cb(lv_event_t *)
{
    if (tile_moved) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (abs(p.x - tile_press_pt.x) > TILE_MOVE_SLOP ||
        abs(p.y - tile_press_pt.y) > TILE_MOVE_SLOP)
        tile_moved = true;
}

static void launch_cb(lv_event_t *e)
{
    if (reorder_fired) { reorder_fired = false; return; }
    if (tile_moved)    { tile_moved    = false; return; }
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

    if (tile_moved) return;             /* a scroll, not a deliberate hold */

    const int n = app_count();
    if (n < 2) return;

    /* A stale order must be materialised before it can be edited, or the swap
     * below would write into an array nobody is reading. */
    if (!order_valid())
        for (int i = 0; i < n; i++) s.app_order[i] = (uint8_t)i;

    int idx = -1;
    for (int i = 0; i < n; i++) if (order_at(i) == app) { idx = i; break; }
    if (idx < 0) return;

    const int prev = (idx - 1 + n) % n;
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

    /*
     * WRAPPING, and scrollable. A single row fitted three apps and silently
     * broke at five: 5 x 118 + 4 x 22 = 678 px of tiles in a 600 px row, with
     * the overflow simply drawn off-screen and no way to reach it. Scripts
     * make the count unbounded (D037), so the drawer has to grow.
     *
     * The row scrolls vertically only. Horizontal scroll would fight the
     * left-edge back gesture for the same finger movement (D029).
     */
    lv_obj_t *row = lv_obj_create(drawer);
    lv_obj_remove_style_all(row);
    /*
     * 332 px is every pixel between the title and the Clock button, and it is
     * a computed number, not a guess: a cell is TILE + 40 = 158, so two rows
     * plus a 12 px gutter need 328. The first cut of this used a round 300,
     * which clipped the second row by 30 px — and since the name label sits in
     * the bottom 20 px of a cell, the fifth app appeared as a nameless blank
     * square. See D041.
     */
    /* PCT wide; height is whatever sits between the title and the strip —
     * a landscape constant here is why portrait could not reach its third
     * row of tiles (v1.29.1). */
    lv_obj_set_size(row, LV_PCT(100), lv_disp_get_ver_res(nullptr) - 56 - 76);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 22, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row, 12, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(row, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < app_count(); i++) {
        const App *app = order_at(i);
        if (!app) continue;

        lv_obj_t *cell = lv_obj_create(row);
        /*
         * NOT decor(): that clears SCROLL_CHAIN, and the chain is how a drag
         * that starts on a tile reaches the row's scroller. With it cleared,
         * scrolling only worked from the 22 px gaps BETWEEN tiles — which is
         * why portrait's third row was unreachable (v1.29.1). The cell keeps
         * the chain, loses everything else.
         */
        lv_obj_remove_style_all(cell);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(cell, TILE, TILE + 40);

        lv_obj_t *tile = lv_obj_create(cell);
        lv_obj_remove_style_all(tile);
        /* remove_style_all does NOT touch flags: a tile is still SCROLLABLE
         * and in the scroll chain by default. D014, again. */
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(tile, TILE, TILE);
        lv_obj_align(tile, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(tile,
            lv_color_hex(theme_get().tile[i % 3]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(tile, 22, LV_PART_MAIN);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, press_cb,  LV_EVENT_PRESSED, nullptr);
        lv_obj_add_event_cb(tile, tile_pressing_cb, LV_EVENT_PRESSING, nullptr);
        lv_obj_add_event_cb(tile, launch_cb, LV_EVENT_CLICKED, (void *)app);
        lv_obj_add_event_cb(tile, reorder_cb, LV_EVENT_LONG_PRESSED, (void *)app);
        if (app->icon) {
            lv_obj_add_event_cb(tile, app->icon, LV_EVENT_DRAW_MAIN, nullptr);
        } else {
            /*
             * Script apps have no icon — the App contract lets it be null and
             * nothing in a .lua file can draw one. An empty charcoal square
             * tells you nothing, so fall back to the first letter of the name.
             * It is not decorative: it is what distinguishes Blink from
             * Uptime at a glance.
             */
            lv_obj_t *g = lv_label_create(tile);
            lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_font(g, &lv_font_montserrat_44, LV_PART_MAIN);
            lv_obj_set_style_text_color(g, lv_color_hex(0x7A7A7A), LV_PART_MAIN);
            char initial[2] = { (char)toupper((unsigned char)app->name[0]), '\0' };
            lv_label_set_text(g, initial);
            lv_obj_center(g);
        }

        lv_obj_t *lbl = lv_label_create(cell);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, COL_TEXT, LV_PART_MAIN);
        lv_label_set_text(lbl, app->name);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    /* The same back chip as every app, in the same corner (D045). It says
     * Back rather than Clock because the label names the GESTURE, not the
     * destination — one word that is always true beats four that rotate. */
    app_host_std_back(drawer, nullptr);

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

        restore_user_rotation();   /* drawer lays out for the owner's shape */

        /* Load the drawer BEFORE deleting the app's screen — deleting the
         * active screen is how you get a use-after-free (D031). */
        release_input();
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
        /* A scroll is not a back gesture. Now that the drawer scrolls, a drag
         * that LVGL claimed for scrolling must not also pop a level. */
        if (indev->proc.types.pointer.scroll_obj) { edge_down = down; return; }
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

/* Set from the web server's task, consumed on the LVGL loop. */
static volatile bool pending_open;
static volatile bool pending_back;
static char          pending_name[24];

void app_host_request_back(void) { pending_back = true; }

bool app_host_request_open(const char *name)
{
    if (!name || !*name) return false;
    if (strcasecmp(name, "clock") != 0 && strcasecmp(name, "drawer") != 0) {
        bool found = false;
        for (int i = 0; i < app_count(); i++) {
            const App *a = app_at(i);
            if (a && a->name && strcasecmp(a->name, name) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    snprintf(pending_name, sizeof(pending_name), "%s", name);
    pending_open = true;
    return true;
}

const char *app_host_current(void)
{
    if (running && running->name) return running->name;
    if (drawer) return "drawer";
    return "clock";
}

static void serve_pending(void)
{
    if (pending_back) {
        pending_back = false;
        app_host_back();
        return;                 /* whatever was running is gone; do no more */
    }
    if (!pending_open) return;
    pending_open = false;

    if (strcasecmp(pending_name, "clock") == 0)  { app_host_home(); return; }
    if (strcasecmp(pending_name, "drawer") == 0) {
        if (app_host_is_open()) app_host_home();
        app_host_open_drawer();
        return;
    }
    for (int i = 0; i < app_count(); i++) {
        const App *a = app_at(i);
        if (a && a->name && strcasecmp(a->name, pending_name) == 0) {
            if (app_host_is_open()) app_host_home();
            app_host_launch(a);
            return;
        }
    }
}

void app_host_tick(void)
{
    back_gesture_tick();
    serve_pending();

    /*
     * Script uploads arrive on the async web server's task and only mark the
     * registry dirty; the rebuild happens HERE, on the LVGL task, and only
     * while nothing is open. Two reasons, both concrete: rebuilding the
     * registry from another task races every app_at() the drawer makes, and
     * rebuilding it while the drawer is up would invalidate the App pointers
     * its tiles carry as event user data. See D037 and D038.
     */
    if (!app_host_is_open()) script_rescan_if_pending();

    if (running && running->tick) running->tick();
}
