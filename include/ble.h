/**
 * ble.h — BLE GATT peripheral for the emotion API.
 *
 * Runs on the NimBLE host task. Write callbacks parse and enqueue; they
 * never touch LVGL. Same contract as the weather task and the HTTP server.
 */
#pragma once
#include <stdbool.h>

void ble_begin(void);                  /* no-op when disabled in settings */
void ble_stop(void);
void ble_apply_name(const char *name); /* restarts advertising */
bool ble_is_running(void);
bool ble_is_connected(void);
