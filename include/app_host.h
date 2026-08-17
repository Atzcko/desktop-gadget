/**
 * app_host.h — the drawer, the registry, and the route home.
 */
#pragma once
#include <stdbool.h>

void app_host_open_drawer(void);
void app_host_home(void);        /* leave whatever is open, back to the clock */
bool app_host_is_open(void);     /* drawer or an app is on screen             */
void app_host_tick(void);        /* from the LVGL loop                        */
