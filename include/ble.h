/**
 * ble.h — BLE GATT peripheral for the emotion API.
 *
 * Runs on the NimBLE host task. Write callbacks parse and enqueue; they
 * never touch LVGL. Same contract as the weather task and the HTTP server.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void ble_begin(void);                  /* no-op when disabled in settings */
void ble_stop(void);
void ble_apply_name(const char *name); /* restarts advertising */
bool ble_is_running(void);
bool ble_is_connected(void);

/* Relative mouse over the HID mouse report (ID 2). Returns false when no
 * host is connected. buttons: bit0 left, bit1 right, bit2 middle (D051). */
bool ble_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);

/* One keystroke on the HID keyboard report (ID 3): press+release. modifiers
 * bit1 = left shift. US usage codes (D052). */
bool ble_key(uint8_t modifiers, uint8_t keycode);

/* How many hosts subscribed to each input report; -1 = report absent.
 * 0 on a connected host means the bond predates the report map (re-pair). */
int ble_mouse_subs(void);
int ble_key_subs(void);

/* Forget every bonded host. Use when macOS pairing gets into a bad state:
 * the Mac remembers a bond the device has forgotten, or vice versa. */
void ble_clear_bonds(void);
