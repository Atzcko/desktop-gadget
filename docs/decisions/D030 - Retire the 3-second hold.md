---
title: D030 - Retire the 3-second hold
type: decision
status: accepted
date: 2026-08-18
tags:
  - decision
  - ui
---

# D030 — Retire the 3-second hold

## Context

Holding a finger on the clock for 3 seconds opened Settings
([[D027 - The gesture budget]]). It was built when Settings was the only other
screen on the device and there was no other way to reach it.

That is no longer true. Settings is an app; the drawer is how you reach apps
([[D026 - Apps are a platform, not a special case]]). The hold had become a
second route to a place that already had one.

D027 warned in as many words that "every gesture makes the next one harder" and
that a fifth should be resisted. The honest reading is that the *first* one to
go should be the one whose reason expired.

## Decision

**The 3-second hold is removed**, along with the accent bar that advertised it.

Settings is reached by swiping up from the bottom edge and tapping its tile.

The clock is left with three gestures, and they are cleanly separated:

| Gesture | Action |
|---|---|
| Tap | date + weather sync age |
| Long press ≥ 1.2 s | cycle brightness |
| Swipe up from the bottom edge | app drawer |

## Consequences

- **The press path is simpler.** Nothing happens *during* a press any more —
  brightness is decided on release by duration. The `PRESSING` branch only
  watches for movement, and `settings_fired` is gone along with the state it
  guarded.
- **Resting a finger on the clock no longer opens anything.** This was the real
  cost of the gesture and the reason it is being spent first.
- **Settings is one tap further away.** It is a screen you visit to provision
  Wi-Fi and a city and then rarely again; the drawer is the right distance for
  it.
- Room in the budget for the gesture that replaced it
  ([[D029 - Back is a system gesture, not a widget event]]).

## Related

- [[D027 - The gesture budget]] — its reasoning stands; this retires one of the
  four it describes
- [[D026 - Apps are a platform, not a special case]]
