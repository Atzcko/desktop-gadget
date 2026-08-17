---
title: Module map
type: reference
tags:
  - reference
  - architecture
updated: 2026-08-17
---

# Module map

Twelve source files and no map was one file too many. This is what each owns,
and — more usefully — what each is **not allowed** to do.

## The one rule that shapes everything

> [!warning] Only the LVGL task draws
> `ble.cpp`, `httpapi.cpp` and the weather task in `net.cpp` all run on other
> FreeRTOS tasks. **None of them may touch an LVGL object.** They validate,
> enqueue, and return; the LVGL loop drains the queue and draws. This has held
> since [[D018 - Emotion API - one engine, two transports]] and is the reason
> this firmware has never hit the classic "LVGL called from two tasks" crash.

## Files

| File | Owns | Must not |
|---|---|---|
| `main.cpp` | boot order, the 200 ms tick, brightness schedule, burn-in walk, host callbacks in `app.h` | grow UI logic — it is the conductor, not a player |
| `ui.cpp` | the **clock screen**: cards, fold animation, gestures, the emotion line, the corner clock, the zoom canvas | know anything about settings or apps |
| `ui_settings.cpp` | the settings screen and the text editor overlay | be reached except through `ui_settings_open()` |
| `emotion.cpp` | the circumplex table, JSON parsing, the queue | draw |
| `net.cpp` | Wi-Fi, NTP, Open-Meteo, geocoding, the weather task | draw |
| `ble.cpp` | NimBLE peripheral, NUS + HID | draw |
| `httpapi.cpp` | `POST /emotion`, `GET /health`, mDNS | draw |
| `settings.cpp` | the NVS-backed store and the timezone table | read `config.h` outside `settings_reset()` |
| `fliqlo_*.c` | four generated fonts: 210, 50, 44, 34/22 px | be edited by hand — regenerate with `lv_font_conv` |

## Boot order, and why it is that order

```
Serial → settings_load() → beginAMOLED_241() → setRotation()
       → beginLvglHelper() → ui_init() → emotion_begin()
       → ble_begin()  ←── BEFORE Wi-Fi, or coexistence aborts
       → net_begin()
```

Two orderings are load-bearing and both were learned by boot loop:

- **BLE before Wi-Fi** — the BT controller must claim the radio first, or
  `coex_core_enable()` aborts. [[D017 - BLE and Wi-Fi coexistence]]
- **`setRotation()` before `beginLvglHelper()`** — and any repaint it triggers
  must be guarded, because `lv_scr_act()` is NULL until LVGL is up.

## Where the size is

`ui.cpp` reached **1333 lines** carrying five separate concerns: the clock, the
gesture state machine, the emotion renderer, the corner clock, and the zoom
canvas. That is the pressure the app platform relieves — see
[[D026 - Apps are a platform, not a special case]].
