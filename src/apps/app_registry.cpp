/**
 * app_registry.cpp — the list. Adding an app is one line here and one file in
 * src/apps/; nothing else in the firmware needs to know it exists.
 */
#include "app_api.h"

extern const App app_timer;

const App *const APPS[]  = { &app_timer };
const int        APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
