---
title: D042 - The battery gauge is a voltage estimate
type: decision
status: accepted
date: 2026-08-23
tags:
  - decision
  - hardware
  - ui
---

# D042 — The battery gauge is a voltage estimate

## Context

The owner asked what a 5000 mAh battery would give, then asked for a battery
indicator: a small battery glyph with the percentage inside. The project had
been explicitly USB-only since Stage 0 ("no battery logic" is in the original
scope), so this is the first battery feature, and the hardware constrains it.

**The SY6970 is a charger, not a fuel gauge.** It has no coulomb counter.
The only battery quantity it can report is the cell voltage from its ADC
(already enabled — `beginAMOLED_241` calls `SY.enableMeasure()`).

## Decision

**Percent is derived from voltage** through a piecewise-linear resting LiPo
curve (4.20 V → 100 %, 3.82 V → 50 %, 3.45 V → 0 %), and the display is
honest about the consequences rather than pretending precision:

- **Under load it reads low; on the charger it reads high.** The CV phase
  holds the cell at 4.2 V long before it is actually full, so a charging
  percentage leans optimistic. The chip turns green while charging partly to
  signal "this number is a charger number".
- **It moves in steps, not smoothly** — a resting curve sampled every 30 s.

The alternative — a proper gauge IC — is a hardware change and out of scope.

## The widget

46×22 chip, top right of the clock, nub outside, percentage inside in
montserrat 14. Three states, one colour each: charging green, under 15 % red,
otherwise the same secondary grey as everything else on the screen.

- **Hidden when no battery is connected** — the normal state of this device.
  A USB-only clock does not wear a battery icon.
- **Hidden in line mode** — the weather strip takes that corner
  ([[D023 - Emotions must change the mode, not decorate it]]).
- A child of `root`, so the burn-in walk moves it with everything else.

## Plumbing

Polled every 30 s **from the loop task**: the PMU shares the internal I²C bus
with the touch controller, so single-master discipline applies to the bus the
way [[D018 - Emotion API - one engine, two transports]] applies it to the
widget tree. `/health` gains
`battery{present, mv, pct, charging, vbus}` — `pct` is −1 without a battery.

## What this is not

Telemetry, not power management. No low-battery shutdown, no brightness
reaction, no deep sleep — the scope boundary in [[index]] still stands except
for the one word "no battery logic", which is now "no battery *management*".

## Related

- [[T4-S3]] — the PMU, and the charge LED it also owns
- [[D018 - Emotion API - one engine, two transports]]
