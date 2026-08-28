/**
 * app_host.h — the drawer, the registry, and the route home.
 */
#pragma once
#include <stdbool.h>
#include <lvgl.h>

struct App;

void app_host_launch(const App *app);   /* straight to an app, no drawer */
void app_host_open_drawer(void);
void app_host_back(void);        /* pop one level: app -> drawer -> clock     */
const struct App *app_host_running(void);  /* the live app, or null            */
void app_host_home(void);        /* leave whatever is open, back to the clock */
bool app_host_is_open(void);     /* drawer or an app is on screen             */
void app_host_tick(void);        /* from the LVGL loop                        */

/*
 * Ask the host to open something, by name, from another task. "clock" routes
 * home. The switch happens on the LVGL loop, never in the caller. This exists
 * because the only way to reach a screen was a finger, which made every UI
 * bug unverifiable from here. See D039.
 */
bool app_host_request_open(const char *name);

/*
 * Ask for back WITHOUT unwinding here. A script's back() runs inside
 * lua_pcall on the very state that app_host_back() would close, so calling it
 * directly closes the interpreter that is still executing. See D041.
 */
void app_host_request_back(void);

/*
 * THE back button — one look, one place, every screen (D045): bottom-left,
 * 132x56, charcoal, "< Back". Pass a custom cb only when leaving needs a
 * guard (Settings' unsaved-changes prompt); null wires app_host_back().
 * Reserve the bottom-left 156x76 of your layout for it.
 */
lv_obj_t *app_host_std_back(lv_obj_t *parent, lv_event_cb_t cb);
const char *app_host_current(void);   /* "clock", or the running app's name */
