---
title: D051 - The clock is a trackpad; the mouse is real HID
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - ble
---

# D051 — The clock is a trackpad; the mouse is real HID

## Context

The owner asked for a mouse-control app: one-finger tap clicks, two-finger
tap right-clicks. The transport question had already been answered in
another decision's context: the device presents BLE HID
([[D022 - Custom HID identity, not a keyboard]]) and the v1 scope note
always said HID *input* was "a later phase". This is that phase arriving —
for the pointer.

## Decision

**A standard relative-mouse collection (Report ID 2) joins BOTH HID report
maps.** A pointing device does not summon Keyboard Setup Assistant, so the
gadget identity keeps its meaning while gaining a cursor; the keyboard
fallback identity gains the same collection. `ble_mouse(buttons,dx,dy,wheel)`
is the whole API — no companion, no protocol, the cursor is system-wide on
whatever host is paired.

**The gesture set is a laptop pad's**, implemented in the app:

| Gesture | Means |
|---|---|
| one finger moves | cursor moves (gain 1.5, fractional carry) |
| still tap < 250 ms | left click |
| **two-finger** tap | right click |
| two-finger drag | scroll wheel |
| strip buttons | explicit Left / Right, for drag-free clicking |

**Two fingers are the one thing LVGL cannot see** — its pointer indev
reports only the first touch. `app_touch_count()` reads the CST226's raw
point count while a press is live. This double-reads the controller the
LVGL helper also polls; watched for, accepted, and hardware-verified by the
owner rather than assumed (the count is read-only state, not a cleared
FIFO).

**A click is two reports** (down, then up ~30 ms later on the app's tick),
and `destroy()` always sends an all-up report — leaving a phantom held
button on the host is the one unforgivable trackpad bug.

## The cost, stated before the owner finds it

> [!warning] The HID descriptor changed — re-pairing is MANDATORY
> Hosts cache the report map per bond (the standing warning in CLAUDE.md).
> Until the Mac forgets the device AND the clock clears its pairings AND
> they re-pair, the mouse reports fall on a host that still believes the old
> descriptor. Nothing will move and nothing will error.

## Related

- [[D022 - Custom HID identity, not a keyboard]] · [[D021 - BLE HID, for discoverability not typing]]
- [[D027 - The gesture budget]] — the app owns its screen, so its gestures
  are its own business
