---
title: Stage 2 - NTP and flip animation
type: stage
stage: 2
status: planned
tags:
  - stage
---

# Stage 2 — NTP and the split-flap flip

**Goal.** Real local time, and the fold animation on minute change.

## Scope

- Wi-Fi connect from `config.h`, non-blocking, with auto-reconnect.
- `configTzTime(TZ_POSIX, "pool.ntp.org")` — POSIX TZ string so DST is handled by libc, not by us.
- Daily resync.
- Split-flap fold on minute rollover, 8–10 frames driven by `lv_anim_t`.

## The animation

Top half of the changing card folds down over the bottom half. Implemented as a scale-Y transform on a clipped copy of the outgoing digit, over ~400 ms with `lv_anim_path_ease_in`, then the incoming digit revealed underneath.

Only the minute card animates on a normal tick; the hour card animates only when the hour changes.

> [!warning] 23:59 → 00:00 is the case that breaks naive implementations
> Both cards change at once, and the hour goes 23 → 00 rather than incrementing. The animation must be driven by *"the rendered digit differs from the target digit"*, per card, never by arithmetic on the previous value. Same rule makes the first post-NTP jump (from whatever the RTC held to real time) animate correctly instead of desyncing.

## Tearing

A single full-screen PSRAM buffer means LVGL composites the whole frame before any of it reaches the panel — see [[D009 - LVGL buffer strategy]]. Tearing would have to come from flushing faster than the panel scans; the TE pin (GPIO 18) exists if it ever proves necessary, but the stock flush path does not use it and is expected to be sufficient.

## Acceptance

- [ ] Cold boot → correct local time on screen in **under 10 s**
- [ ] Minute rollover animates
- [ ] 23:59 → 00:00 animates correctly on both cards
- [ ] No tearing during the fold
- [ ] Timezone and DST correct for the configured POSIX string

## Next

[[Stage 3 - Weather]]
