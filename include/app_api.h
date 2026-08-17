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
};

extern const App *const APPS[];
extern const int        APP_COUNT;
