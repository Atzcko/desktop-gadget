/**
 * ui_settings.h — the on-device settings screen.
 *
 * Opened by holding a finger on the clock for 3 seconds. Everything it
 * changes is written to NVS, so it survives reboots and re-flashes.
 */
#pragma once
#include <stdbool.h>

void ui_settings_open(void);
bool ui_settings_is_open(void);
