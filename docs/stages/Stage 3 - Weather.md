---
title: Stage 3 - Weather
type: stage
stage: 3
status: awaiting-hardware-verification
tags:
  - stage
---

# Stage 3 — Weather

**Goal.** Current temperature plus today's min/max from Open-Meteo, fetched off the render path.

## Scope

- Open-Meteo forecast endpoint, no API key, `current=temperature_2m` + `daily=temperature_2m_max,temperature_2m_min`, coordinates and units from `config.h`.
- A **dedicated FreeRTOS task** owns the fetch. It never touches LVGL objects directly — it parses into a struct guarded by a mutex, and the LVGL tick loop reads it. This is the rule that keeps rendering unblocked and avoids the classic "LVGL called from two tasks" crash.
- Refresh every 15 min.
- On failure: keep the last good values, show a subtle stale dot, retry with exponential backoff.

## Typography

The brief caps this at **two type sizes besides the clock digits**:

1. Current temperature — the largest weather element.
2. Min/max — the smaller size, beside or below it.

The stale dot is a shape, not a third type size. The optional emotion message from [[Stage 4 - Emotion API]] reuses the *smaller* of the two.

## Outcome — 2026-08-16

Implemented. A dedicated FreeRTOS task (`weather`, 6 KB stack, core 0) owns the fetch and **never touches an LVGL object** — it writes into a mutex-guarded struct and the LVGL loop polls `net_weather()`. Same discipline the emotion API will need.

- Sleeps in **250 ms slices** rather than one long block, so "fetch now" from the Settings screen is honoured promptly instead of up to 15 minutes later.
- Backoff doubles from 30 s and is **capped at the normal 15-minute interval** — a failing endpoint must never end up retried less often than a healthy one.
- On failure the last good values stay on screen and a small amber dot appears in the weather row. The dot is a **shape, not a third type size**, which keeps the brief's two-size cap intact.
- HTTPS with `setInsecure()`. Pinning a CA would mean shipping and rotating a root bundle on a device with no update path, for public forecast data that carries none of our credentials.

## Acceptance

- [ ] Temperature and min/max render
- [ ] Clock keeps ticking and animating during a fetch
- [ ] Wi-Fi pulled → last value retained, stale dot appears, no crash, no frozen clock
- [ ] Wi-Fi restored → values refresh, dot clears
- [ ] Backoff visible in the serial log rather than a tight retry loop

## Next

[[Stage 4 - Emotion API]]
