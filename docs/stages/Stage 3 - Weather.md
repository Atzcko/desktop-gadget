---
title: Stage 3 - Weather
type: stage
stage: 3
status: planned
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

## Acceptance

- [ ] Temperature and min/max render
- [ ] Clock keeps ticking and animating during a fetch
- [ ] Wi-Fi pulled → last value retained, stale dot appears, no crash, no frozen clock
- [ ] Wi-Fi restored → values refresh, dot clears
- [ ] Backoff visible in the serial log rather than a tight retry loop

## Next

[[Stage 4 - Emotion API]]
