---
title: Desktop gadget — Fliqlo flip clock
type: index
device: LilyGO T4-S3
status: feature-complete-pending-verification
updated: 2026-08-16
tags:
  - index
---

# Desktop gadget — T4-S3 flip clock

Firmware turning a **LilyGO T4-S3** into a minimalist Fliqlo-style flip clock with weather and an HTTP "emotion" API that lets Claude Code animate the device during a working session.

This vault is the project's memory: every decision, why it was made, and what happened at each stage. Vault root = project root, so the firmware and its documentation live together.

## Current state

> [!success] Online and feature-complete
> Wi-Fi joined, NTP synced, weather live for **Abu Dhabi** (32.0 °C, 30.5–40.7). `flipclock.local` resolves and both `GET /health` and `POST /emotion` are verified end-to-end, including clean 400s on bad input. BLE advertises as **"Flip Clock"**. Only the BLE transport remains unverified — see [[Stage 4 - Emotion API]].

| Stage | What | Status |
|---|---|---|
| [[Stage 0 - Stock example]] | Stock library example runs | ✅ built + flashed, board confirmed |
| [[Stage 1 - Static digits]] | Fliqlo layout, hardcoded time | ✅ built + flashed, needs visual check |
| [[Stage 2 - NTP and flip animation]] | Real time + split-flap fold | ✅ flashed, needs visual check |
| [[Settings screen]] | 3-second hold → full settings | ✅ flashed, needs visual check |
| [[Stage 3 - Weather]] | Open-Meteo on its own task | ✅ flashed, needs Wi-Fi |
| [[Stage 5 - Touch and burn-in guard]] | Touch gestures + pixel walk | ✅ flashed, needs visual check |
| [[Stage 4 - Emotion API]] | HTTP + **BLE** + `CLAUDE.md` | ✅ flashed, BLE unverified |

## Decisions

| # | Decision |
|---|---|
| [[D001 - Use the LilyGO AMOLED library, not TFT_eSPI]] | QSPI panel — TFT_eSPI physically cannot drive it |
| [[D002 - Pin LVGL to 8.4.0]] | Exact pin; LVGL 9 is a port, not an upgrade |
| [[D003 - Copy the library platformio env verbatim]] | Every build flag is load-bearing |
| [[D004 - Rotation 0 is already landscape]] | 600×450 is the default — no `setRotation` needed |
| [[D005 - PlatformIO runs on Python 3.12]] | System Python is 3.14, too new for PIO 6.x |
| [[D006 - Keep build artifacts out of iCloud]] | `.pio` in iCloud is a sync storm + eviction hazard |
| [[D007 - Skip SD, keep charge LED default]] | `beginAMOLED_241(true, false)` |
| [[D008 - Brightness scale]] | 0–255 range; day 90 ≈ 35 %, night 25 |
| [[D009 - LVGL buffer strategy]] | Full-screen 527 KB buffer in PSRAM |
| [[D010 - Night dimming is a schedule, not a sensor]] | T4-S3 has no ambient light sensor |
| [[D011 - Generate the digit font, do not scale Montserrat]] | Montserrat stops at 48 px; LVGL 8 can't scale labels |
| [[D012 - Card geometry]] | Every constant derives from the 116.8 px digit advance |
| [[D013 - Settings live in NVS, config.h is only defaults]] | On-device settings; NVS survives re-flashing |
| [[D014 - Touch hit-testing]] | Every LVGL object is CLICKABLE by default — strip it |
| [[D015 - Abu Dhabi locale]] | `<+04>-4`; brackets required, offset sign inverted |
| [[D016 - Wi-Fi is provisioned on-device]] | The password never leaves the owner's hands |
| [[D017 - BLE and Wi-Fi coexistence]] | One radio: BLE must init first, modem sleep is mandatory |
| [[D018 - Emotion API - one engine, two transports]] | Shared engine, NUS over BLE, same JSON |
| [[D019 - Text entry gets its own screen]] | `LV_EVENT_FOCUSED` is an edge, not a level |
| [[D020 - Humidity recedes by contrast, not size]] | Type-size budget spent; use luminance |
| [[D021 - BLE HID, for discoverability not typing]] | Only a HID profile gets you into macOS Bluetooth settings |
| [[D022 - Custom HID identity, not a keyboard]] | The report descriptor, not the appearance, defines the device |
| [[D023 - Emotions must change the mode, not decorate it]] | Minimal means few elements, not low contrast |
| [[D024 - Emotions as a circumplex, rendered as one line]] | 26 emotions, one renderer: valence→hue, arousal→agitation |

## Releases

- [[RELEASES]] — **v1.1.0**, 2026-08-16. Bump `include/version.h`, log it, tag it, flash it, confirm via `/health`.

## Reference

- [[T4-S3]] — pins, panel constants, rotation table, verified USB IDs
- [[LilyGo AMOLED library]] — the API surface we actually depend on
- [[CLAUDE]] — build/flash commands and working agreements
- [log.md](log.md) — chronological project lifecycle

## Scope boundaries

> [!warning] Explicitly out of scope for v1
> - **BLE HID input** (actually sending keystrokes/media keys) — still a later phase. The device now *presents* a HID keyboard service so macOS will pair with it, but sends no reports: [[D021 - BLE HID, for discoverability not typing]].
> - **Battery / deep sleep** — USB-powered always-on.
> - **SD card** — slot unused, init skipped.
> - **Any cloud beyond Open-Meteo and NTP.** All rendering is local.

## Acceptance criteria for v1 — see [[Acceptance results]]

- [x] Cold boot → correct local time in under 10 s — **4.45 s**
- [ ] Minute rollover, including 23:59 → 00:00 — **needs your eyes**
- [x] `POST /emotion` animates then reverts — **5.2 s**, verified via `/health`
- [x] Wi-Fi drop → clock runs, weather degrades, auto-reconnects — observed in the field
