---
title: Module map
type: reference
tags:
  - reference
  - architecture
updated: 2026-08-22
---

# Module map

Sixteen source files, ~5300 lines. This is what each owns, and — more
usefully — what each is **not allowed** to do.

## The one rule that shapes everything

> [!warning] Only the LVGL task draws
> `ble.cpp`, `httpapi.cpp` and the weather task in `net.cpp` all run on other
> FreeRTOS tasks. **None of them may touch an LVGL object.** They validate,
> enqueue, and return; the LVGL loop drains the queue and draws. This has held
> since [[D018 - Emotion API - one engine, two transports]] and is the reason
> this firmware has never hit the classic "LVGL called from two tasks" crash.
> The OTA upload obeys it too: it writes flash from the async task but speaks
> to the screen only through the emotion queue.

## The host and the apps

| File | Owns | Must not |
|---|---|---|
| `include/app_api.h` | the six-field `App` contract: name, icon, create, destroy, tick, back | grow — every field is a promise every app must keep |
| `app_host.cpp` (314) | the drawer, launch, **`app_host_back()`** (the one meaning of "back" — [[D033 - Back goes one level, not home]]), the polled left-edge gesture ([[D029 - Back is a system gesture, not a widget event]]), tile reorder ([[D031 - The layout is the model]]) | know what any app draws |
| `apps/app_registry.cpp` (16) | the `APPS[]` list — adding an app is one line here, one file there | encode display order; that lives in NVS (`app_order`) |
| `apps/app_timer.cpp` (486) | countdown timer: per-card rollers with tap/hold fine-set ([[D028 - Set a number by dragging the number]], [[D032 - Three gestures, one control]]), its own copy of the fold | share the fold with ui.cpp until a third caller exists |
| `apps/app_settings.cpp` (44) | the adapter that puts Settings in the drawer | contain settings UI — that stays in `ui_settings.cpp` |
| `script.cpp` (564) | the Lua runtime: PSRAM allocator, instruction budget, `ui`/`gpio` bindings, the LittleFS script registry ([[D037 - Apps become Lua scripts]]) | load `io`, `os`, `package` or `debug` — each is a way out of the sandbox |
| `apps/app_registry.cpp` | native apps **plus** scripts, natives first so an index never shifts | be an array again |
| `apps/app_lab.cpp` (463) | GPIO / I²C / UART bench tool on the whitelist ([[D035 - The Lab may only touch pins the firmware does not own]]) | touch a pin outside `PINS[]`; leave anything configured on exit |

## The rest

| File | Owns | Must not |
|---|---|---|
| `main.cpp` (260) | boot order, the loop, brightness schedule, burn-in walk | grow UI logic — conductor, not player |
| `ui.cpp` (1346) | the **clock screen**: cards, fold, clock gestures (tap / long-press / swipe-up), emotion line, corner clock, zoom canvas | know anything about settings or apps |
| `ui_settings.cpp` (947) | the settings screen + text editor overlay; answers `back()` by closing the editor | be reached except through the host |
| `emotion.cpp` (201) | the 32-state circumplex table, JSON parsing, the queue | draw |
| `net.cpp` (375) | Wi-Fi, NTP, Open-Meteo, geocoding, the weather task | draw |
| `ble.cpp` (359) | NimBLE peripheral, NUS + vendor HID ([[D022 - Custom HID identity, not a keyboard]]) | draw |
| `httpapi.cpp` (238) | `GET /health`, `POST /emotion`, **`POST /update`** ([[D034 - Updates ship over the air]]), mDNS | draw |
| `settings.cpp` (220) | the NVS store, timezone table, `app_order` | read `config.h` outside `settings_reset()` |
| `fliqlo_*.c` | four generated fonts: 210 / 50 / 44 / 22 px | be edited by hand — regenerate with `lv_font_conv` |

Outside `src/`: `scripts/version_stamp.py` injects `FW_GIT`;
`tools/ota` is the release path ([[D034 - Updates ship over the air]]);
`tools/say` / `tools/hush` wrap the global `flipclock` command.

## Boot order, and why it is that order

```
Serial → settings_load() → beginAMOLED_241() → setRotation()
       → beginLvglHelper() → ui_init() → emotion_begin()
       → ble_begin()  ←── BEFORE Wi-Fi, or coexistence aborts
       → net_begin()          (httpapi starts on first association)
```

Two orderings are load-bearing and both were learned by boot loop:

- **BLE before Wi-Fi** — the BT controller must claim the radio first, or
  `coex_core_enable()` aborts. [[D017 - BLE and Wi-Fi coexistence]]
- **`setRotation()` before `beginLvglHelper()`** — and any repaint it triggers
  must be guarded, because `lv_scr_act()` is NULL until LVGL is up.

The loop: `lv_timer_handler()` → `emotion_tick()` → `app_host_tick()` (which
polls the back gesture above the widget tree, then ticks the running app).

## Navigation, complete

```
clock ──swipe up──▶ drawer ──tap tile──▶ app
  ▲                   │ ▲                  │
  └──── back ─────────┘ └────── back ──────┘
        (left-edge swipe or button — one level per pop, D033)
```

The clock itself keeps three gestures: tap (date + sync age), long press
(brightness), swipe up (drawer). The 3-second hold is gone
([[D030 - Retire the 3-second hold]]).
