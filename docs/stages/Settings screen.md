---
title: Settings screen
type: stage
stage: 2.5
status: awaiting-hardware-verification
date_started: 2026-08-16
tags:
  - stage
  - settings
---

# Settings screen — hold 3 seconds

Added at the owner's request, outside the original stage plan. See [[D013 - Settings live in NVS, config.h is only defaults]].

## Gesture map

The brief originally specified "long press → cycle brightness. Nothing else." That still holds; the hold-to-Settings gesture is layered **above** it rather than replacing it, so nothing from the original spec was lost.

| Gesture | Duration | Action |
|---|---|---|
| Tap | < 400 ms | date + weather sync age for 5 s |
| Long press | 1.2 s – 3 s | cycle brightness `{25, 60, 90, 140, 200}` |
| **Hold** | **≥ 3 s** | **open Settings** |

> [!tip] A silent hold is an invisible affordance
> Nobody discovers a gesture with no feedback — they let go early and conclude it is broken. So a thin accent bar appears at the bottom of the screen at 1.2 s and fills to full width at the trigger point. It doubles as the brightness-cycle indicator, and it is hidden the instant the finger lifts.

> [!note] Shortened to 3 s on 2026-08-16
> Originally 5 s. The owner found it too long in use. `SETTINGS_MS` in `ui.cpp` is the single constant; the hold bar derives its fill span from it, so changing one number keeps the indicator honest.

## Tabs

| Tab | Contents |
|---|---|
| **Wi-Fi** | live scan, pick network, password keyboard, connect — [[D016 - Wi-Fi is provisioned on-device]] |
| **Time** | timezone roller (21 zones), 24-hour switch, apply & resync NTP |
| **Place** | city search via Open-Meteo geocoding → sets lat/lon |
| **Screen** | day/night brightness sliders (live preview), night start/end hours, weather on/off, burn-in guard on/off |
| **Info** | mDNS host, SSID, IP, RSSI, NTP state, weather sync age, uptime, free PSRAM/heap, reset to defaults |

## Design notes

- Built on **its own LVGL screen** (`lv_obj_create(NULL)`), so the clock screen is never disturbed and returns exactly as it was. The settings screen is deleted on close.
- The clock loop **skips all its work** while Settings is open (`ui_settings_is_open()`), so no flip animation fires behind the menu and the burn-in walk does not move a screen that is not visible.
- Day brightness previews **live** as the slider moves — a brightness control you cannot see the effect of is useless.
- City search uses Open-Meteo's **geocoding** endpoint. This stays inside the brief's "no cloud beyond Open-Meteo and NTP" constraint: same provider, no key, no account.

## Acceptance

- [x] Builds and boots clean
- [ ] Hold anywhere for 3 s opens Settings — **including on top of a card**, see [[D014 - Touch hit-testing]]
- [ ] Hold progress bar appears at ~1.2 s
- [ ] Wi-Fi scan lists networks; connecting works; survives reboot
- [ ] Timezone roller changes the clock
- [ ] City search returns Abu Dhabi and sets coordinates
- [ ] Brightness sliders preview live and persist
- [ ] Save & close returns to the clock unchanged
