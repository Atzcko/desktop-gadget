/**
 * ui_settings.h — the on-device settings screen.
 *
 * Opened by holding a finger on the clock for 3 seconds. Everything it
 * changes is written to NVS, so it survives reboots and re-flashes.
 */
#pragma once
#include <stdbool.h>
#include <lvgl.h>

void      ui_settings_open(void);      /* legacy entry: launches it as an app */
bool      ui_settings_is_open(void);

/* The App contract: build the screen, and free what was built. The host owns
 * loading it and deleting it. */
lv_obj_t *ui_settings_create(void);
void      ui_settings_destroy(void);
