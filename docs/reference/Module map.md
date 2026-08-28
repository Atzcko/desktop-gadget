---
title: Module map
type: reference
tags:
  - reference
  - architecture
updated: 2026-08-22
---

# Module map

Twenty-four source files, ~9000 lines. What each owns, and what it may not
do. The one rule is unchanged and unbroken:

> [!warning] Only the LVGL task draws
> The weather task, the web server, BLE, and the three workers (messages,
> YouTube, scripts' uploads) never touch an LVGL object. They validate,
> enqueue or flag, and the LVGL loop draws. [[D018 - Emotion API - one engine, two transports]].

## The host and the apps

| File | Owns |
|---|---|
| `include/app_api.h` | the seven-field `App` contract (name, icon, create, destroy, tick, back, portrait_ok) |
| `app_host.cpp` | drawer (wrapping, scrolling), launch, `app_host_back()` ([[D033 - Back goes one level, not home]]), the polled edge gesture (D029), borrowed-landscape for untagged scripts (D043/D045), the std back chip (D045), reorder (D031) |
| `apps/app_registry.cpp` | natives + scripts as one list |
| `apps/app_timer.cpp` | countdown; per-card rollers (D028/D032), both shapes |
| `apps/app_settings.cpp` + `ui_settings.cpp` | Apple-style nav (D048), row rules, editor overlay (D019) |
| `apps/app_lab.cpp` | GPIO/I²C/UART bench on the whitelist (D035/D036) |
| `apps/app_messages.cpp` + `msg.cpp` | conversations, contacts, unread badge feed (D046/D047) |
| `apps/app_themes.cpp` + `theme.cpp` | the color tables (D049) |
| `apps/app_youtube.cpp` + `yt.cpp` | Data API dashboard, thumbnails via LVGL's tjpgd, modes (D050) |
| `apps/app_trackpad.cpp` | BLE mouse gestures, raw two-finger count (D051) |
| `apps/app_keyboard.cpp` | BLE typing, ASCII→usage (D052) |
| `script.cpp` + `lib/lua` | the Lua runtime, sandboxed bindings (D037, D040, D041) |

## Core

| File | Owns |
|---|---|
| `main.cpp` | boot order, the loop, BOOT-button rotation (D043), battery poll → `gauge.cpp` (D044), badge poll, pending rebuild/rotation service |
| `ui.cpp` | clock screen both shapes, weather, emotion line, battery chip, unread badge, tap overlay with weather yield, re-entrant `ui_init` |
| `emotion.cpp` | the 32-state circumplex + queue |
| `net.cpp` | Wi-Fi, NTP, Open-Meteo task |
| `ble.cpp` | NimBLE: NUS + HID (vendor + mouse + keyboard — D022 rev, D051, D052) |
| `httpapi.cpp` | the whole [[HTTP API]] + mDNS identities |
| `settings.cpp` / `gauge.cpp` / `theme.cpp` / `msg.cpp` / `yt.cpp` | NVS-backed state and workers, per their decisions |

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
