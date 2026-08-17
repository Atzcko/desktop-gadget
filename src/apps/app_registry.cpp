/**
 * app_registry.cpp — the list. Adding an app is one line here and one file in
 * src/apps/; nothing else in the firmware needs to know it exists.
 *
 * This is REGISTRY order, which is not display order — the drawer renders in
 * the owner's order from settings.app_order, so rearranging tiles never has to
 * touch code.
 */
#include "app_api.h"

extern const App app_timer;
extern const App app_settings;

const App *const APPS[]  = { &app_timer, &app_settings };
const int        APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
