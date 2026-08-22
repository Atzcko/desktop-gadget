/**
 * script.h — Lua apps, added and removed without a reboot.
 *
 * A script app is a .lua file in LittleFS at /apps/<name>.lua. It implements
 * the same lifecycle the native App contract does, as globals:
 *
 *     function on_create()  end   -- build your screen (required)
 *     function on_tick()     end  -- optional, every LVGL loop
 *     function on_back()     end  -- optional, return true to consume
 *
 * The runtime creates a FRESH lua_State per launch and closes it on exit, so
 * "create on entry, destroy on exit" holds for scripts exactly as it does for
 * native apps — there is no state to leak between runs. See D037.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct App;

void script_begin(void);           /* mount LittleFS, first scan          */
int  script_rescan(void);          /* re-read /apps; returns script count  */

int         script_count(void);
const App  *script_at(int i);

/* Storage. Names are the filename stem; the runtime rejects anything with a
 * slash or a dot so a name can never escape /apps/. */
bool script_save(const char *name, const uint8_t *body, size_t len, char *err, size_t errcap);
bool script_delete(const char *name);
void script_list_json(char *out, size_t cap);
