---
title: D029 - Back is a system gesture, not a widget event
type: decision
status: accepted
date: 2026-08-17
tags:
  - decision
  - ui
  - apps
---

# D029 — Back is a system gesture, not a widget event

## Context

Every app needs a way home. The Timer and the drawer both grew a **Clock**
button, which works and is discoverable, but it is a button per app — a rule
each new app has to remember, in a contract whose whole point is that apps
cannot break the host ([[D026 - Apps are a platform, not a special case]]).

The owner asked for the phone gesture: **swipe in from the left edge to go
home.**

The obvious implementation is an event handler on the app's screen, the way the
clock handles its own gestures. It does not work. **LVGL 8 does not bubble
events**, so any clickable child eats the press before the screen sees it:

- the Timer's minutes card, which is a roller and must stay clickable
- the Settings tab bar, its lists and its rollers
- every button on both

So the gesture would work on empty background and fail on exactly the widgets
you are most likely to be touching. Setting `LV_OBJ_FLAG_EVENT_BUBBLE` on every
object an app ever creates is not a rule anyone can keep — it is
[[D014 - Touch hit-testing]] again, with a new way to forget.

## Decision

**The back gesture is polled from the LVGL loop, above the widget tree**, in
`app_host_tick()`. Not an event handler anywhere.

That is where a system gesture belongs. Apps get it for free, no app can
swallow it, and it is one fewer of the four contract rules that depends on an
author remembering something.

It mirrors the clock's swipe-up: armed by **where it starts** (within 44 px of
the left edge), judged **on release** (≥ 110 px rightward, ≤ 90 px of vertical
wander). Consistency with the existing swipe matters more than being
marginally more responsive.

Two details that are not obvious:

- **`indev->proc.state`**, because LVGL 8 has no `lv_indev_get_state()`. One
  field, from a public struct.
- **The last point seen while pressed is what gets judged**, not the point read
  after release. Reading after release trusts the touch driver to leave valid
  coordinates behind, and not all of them do — a driver that zeroes the point
  would make the gesture silently impossible to perform.

## The app gets first refusal

The contract gains one optional field:

```c
bool (*back)(void);   /* return true if you consumed it */
```

The host calls it before routing home. It exists for a specific failure:
Settings opens its text editor as a **child of its own screen**, so
`lv_scr_act()` cannot tell the host that typing is in progress, and a stray edge
swipe would throw away a hand-typed Wi-Fi password — the most expensive input on
this device, and the one users are least willing to repeat.

`ui_settings_back()` closes the editor and **keeps** the text. That is not what
"back" conventionally means, but closing the overlay commits nothing to NVS —
the real commit is Join or Save — so keeping it costs nothing and losing it
costs a password. Cancel is still one tap away, and it is labelled.

The field is additive: existing five-field initializers still compile, with
`back` value-initialized to null.

## The Timer had to stop fighting it

The minutes card spans x = 14…282, so it overlaps the edge zone. A back swipe
starting on it would drag the roller a few steps on the way out — and
`set_seconds` is a static that outlives the screen, so you would return to a
timer set to a number you never chose.

The roller now abandons a drag that turns mostly sideways **and puts the value
back**. That is a better roller independently of the gesture: vertical control,
vertical input.

## Consequences

- Back works from anywhere in any app, including on top of widgets.
- The per-app **Clock** buttons stay. The gesture is invisible; the button is
  the discoverable path, and removing it would trade a working affordance for
  a hidden one.
- The host now polls every LVGL frame. It is two comparisons when nothing is
  open, and it returns immediately if no pointer device exists.
- One more thing that reads `indev->proc` directly. If LVGL is ever unpinned
  from 8.4.0 this is a place to check ([[D002 - Pin LVGL to 8.4.0]]).

## Related

- [[D027 - The gesture budget]] — the clock's four gestures, unchanged; this
  one lives only while an app or the drawer is open
- [[D026 - Apps are a platform, not a special case]]
- [[D014 - Touch hit-testing]]
