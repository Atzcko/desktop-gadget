/**
 * app_api.h — the contract every app implements.
 *
 * Deliberately tiny. It is the shape ui_settings.cpp already had before it was
 * named: build your own screen, tear it down on exit, and let the host route
 * back home. See D026.
 *
 * FOUR RULES, each learned the hard way elsewhere in this codebase:
 *   1. create() on entry, destroy() on exit — nothing stays resident.
 *   2. Never touch the clock's objects; the host mediates.
 *   3. decor() every decorative object, or the host's gestures break from
 *      inside your app (D014).
 *   4. Only draw from the LVGL task (D018).
 */
#pragma once

#include <lvgl.h>

struct App {
    const char *name;
    void      (*icon)(lv_event_t *e);   /* LV_EVENT_DRAW_MAIN on a square tile */
    lv_obj_t *(*create)(void);          /* build and return the screen         */
    void      (*destroy)(void);         /* free everything create() allocated  */
    void      (*tick)(void);            /* optional; from the LVGL loop        */

    /*
     * Optional. The host calls this FIRST when the back gesture fires (swipe
     * in from the left edge). Return true if you handled it — an app with a
     * modal or a sub-screen closes that instead of the whole app. Return
     * false, or leave it null, and the host routes home.
     *
     * It exists because Settings opens a full-screen text editor as a CHILD of
     * its own screen, so lv_scr_act() cannot tell the host that typing is in
     * progress. Without this hook a stray edge swipe throws away a hand-typed
     * Wi-Fi password, which is the most expensive input on the device.
     */
    bool      (*back)(void);
};

/*
 * The list is a FUNCTION PAIR, not an array, because it is no longer fixed at
 * link time: native apps are joined by Lua scripts discovered in LittleFS, and
 * uploading one must not need a reboot. See D037.
 *
 * app_at() is valid until the next script_rescan(); the drawer is rebuilt on
 * every visit (D033), so it never holds one across a change.
 */
int        app_count(void);
const App *app_at(int i);
