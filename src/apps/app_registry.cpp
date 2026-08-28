/**
 * app_registry.cpp — the list. Adding an app is one line here and one file in
 * src/apps/; nothing else in the firmware needs to know it exists.
 *
 * This is REGISTRY order, which is not display order — the drawer renders in
 * the owner's order from settings.app_order, so rearranging tiles never has to
 * touch code.
 */
#include "app_api.h"
#include "script.h"

extern const App app_timer;
extern const App app_settings;
extern const App app_lab;
extern const App app_messages;

/* Compiled in. These can never fail to load, which is why Settings is one of
 * them — the screen that fixes a broken Wi-Fi config must not depend on a
 * filesystem. */
static const App *const NATIVE[] = { &app_timer, &app_settings, &app_lab, &app_messages };
static const int        NATIVE_N = sizeof(NATIVE) / sizeof(NATIVE[0]);

/* Native first, then scripts. Stable ordering matters: settings.app_order
 * holds indices into this list, and a native app must never shift because a
 * script was added or removed. */
int app_count(void) { return NATIVE_N + script_count(); }

const App *app_at(int i)
{
    if (i < 0) return nullptr;
    if (i < NATIVE_N) return NATIVE[i];
    return script_at(i - NATIVE_N);
}

int app_native_count(void) { return NATIVE_N; }
