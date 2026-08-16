---
title: Stage 5 - Touch and burn-in guard
type: stage
stage: 5
status: awaiting-hardware-verification
tags:
  - stage
---

# Stage 5 — Touch and AMOLED care

**Goal.** The last two features, and the ones that make it liveable as an always-on desk object.

## Anti burn-in

The whole layout shifts on a **slow random walk within ±2 px**, stepping every few minutes. Implemented as an offset applied to the root container, so every child moves together and no layout code needs to know about it.

Constraints:

- Step is ±1 px at a time, bounded to ±2 px total — a random *walk*, not a random *jump*, so it is never perceptible.
- Prefer **even** offsets where possible so LVGL's `lv_rounder_cb` does not enlarge redraw regions — see [[D009 - LVGL buffer strategy]].
- Card geometry leaves ≥2 px of margin at every edge so the walk can never clip.

A pure-black Fliqlo background is already most of the burn-in defence: the majority of pixels are genuinely off. The walk protects the digits and card edges, which are the parts that actually sit lit for months.

## Touch

Deliberately minimal — the brief says "nothing else":

| Gesture | Action |
|---|---|
| Single tap | show date + weather sync age for 5 s |
| Long press | cycle brightness through `{25, 60, 90, 140, 200}` |

Touch comes through the LVGL indev the library registered, so gesture detection is LVGL event handling, not raw `getPoint()` polling.

> [!note] Manual brightness vs the schedule
> A long-press change holds until the next day/night boundary, then the schedule resumes — [[D010 - Night dimming is a schedule, not a sensor]]. Nothing is persisted to NVS.

## Outcome — 2026-08-16

Brought forward and implemented alongside the Settings screen, since both needed the same gesture plumbing.

- Burn-in walk: ±1 px per step, clamped to ±2 px, every `BURNIN_STEP_SECONDS` (default 180). Applied by moving the single layout root, so no child needs to know it exists. Toggleable in Settings.
- Tap shows date + weather sync age for 5 s, in the smaller weather type size.
- Long press (1.2–3 s) cycles brightness; the hold bar doubles as its indicator.
- A real bug was caught here before flashing — see [[D014 - Touch hit-testing]].

## Acceptance

- [ ] Tap shows date + sync age, auto-hides after 5 s
- [ ] Long press cycles brightness and wraps
- [ ] Layout drift is imperceptible in normal use but measurable over an hour
- [ ] Drift never clips a card edge
- [ ] Neither gesture disturbs the minute flip

## Next

Feature-complete for v1. BLE/HID is explicitly out of scope — see [[index]].
