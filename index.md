---
title: Desktop gadget — Fliqlo flip clock
type: index
device: LilyGO T4-S3
status: stage-0-complete
updated: 2026-08-16
tags:
  - index
---

# Desktop gadget — T4-S3 flip clock

Firmware turning a **LilyGO T4-S3** into a minimalist Fliqlo-style flip clock with weather and an HTTP "emotion" API that lets Claude Code animate the device during a working session.

This vault is the project's memory: every decision, why it was made, and what happened at each stage. Vault root = project root, so the firmware and its documentation live together.

## Current state

> [!success] Stage 0 complete — awaiting your visual check
> Stock library example built, flashed, and confirmed by serial as `LilyGo AMOLED 2.41 inch`. Touch controller ACKed. Panel-lit and touch-responds still need a human eye. See [[Stage 0 - Stock example]].

| Stage | What | Status |
|---|---|---|
| [[Stage 0 - Stock example]] | Stock library example runs | ✅ built + flashed, needs visual check |
| [[Stage 1 - Static digits]] | Fliqlo layout, hardcoded time | ⏸ blocked on Stage 0 sign-off |
| [[Stage 2 - NTP and flip animation]] | Real time + split-flap fold | ⚪ planned |
| [[Stage 3 - Weather]] | Open-Meteo on its own task | ⚪ planned |
| [[Stage 4 - Emotion API]] | HTTP API + `CLAUDE.md` | ⚪ planned |
| [[Stage 5 - Touch and burn-in guard]] | Touch gestures + pixel walk | ⚪ planned |

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

## Reference

- [[T4-S3]] — pins, panel constants, rotation table, verified USB IDs
- [[LilyGo AMOLED library]] — the API surface we actually depend on
- [[CLAUDE]] — build/flash commands and working agreements
- [log.md](log.md) — chronological project lifecycle

## Scope boundaries

> [!warning] Explicitly out of scope for v1
> - **BLE / HID** — that is a later phase. Wi-Fi only.
> - **Battery / deep sleep** — USB-powered always-on.
> - **SD card** — slot unused, init skipped.
> - **Any cloud beyond Open-Meteo and NTP.** All rendering is local.

## Acceptance criteria for v1

- [ ] Cold boot → correct local time on screen in under 10 s
- [ ] Minute rollover animates, including 23:59 → 00:00
- [ ] `POST /emotion` with `celebrate` animates then reverts
- [ ] Wi-Fi drop → clock keeps running, weather degrades gracefully, auto-reconnects
