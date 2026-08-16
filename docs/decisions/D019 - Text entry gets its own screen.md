---
title: D019 - Text entry gets its own screen
type: decision
id: D019
date: 2026-08-16
status: accepted
origin: owner, from hardware use
tags:
  - decision
  - lvgl
  - ux
---

# D019 — Text entry gets its own full-screen editor

The owner reported two faults with the first keyboard implementation. They had one design cause between them.

## Fault 1 — you could not see what you were typing

`lv_keyboard` was created on the settings screen, 220 px tall, aligned to the bottom, floating over the tab content. On a 450 px panel that is half the screen. A textarea laid out low in a tab — the Wi-Fi password field sits under a scan list — ended up **behind the keyboard**.

## Fault 2 — a dismissed keyboard could not be brought back

It was shown from `LV_EVENT_FOCUSED`:

```cpp
lv_obj_add_event_cb(ta_pass, ta_event, LV_EVENT_FOCUSED, nullptr);
```

`LV_EVENT_FOCUSED` fires when an object **gains** focus. Once the textarea already had focus, tapping it again fired nothing, so the keyboard stayed hidden. The only way back was to switch tabs — which moved focus away — and return. Exactly the behaviour reported.

> [!warning] `FOCUSED` is not "tapped"
> For anything that must respond to *every* tap, use `LV_EVENT_CLICKED`. `LV_EVENT_FOCUSED` is an edge, not a level, and it is a very easy trap when a widget is both focusable and clickable.

## Decision

Text entry moves to a **dedicated full-screen editor overlay**, rather than patching the sizes and adding a second event handler.

```
 y=10   title              ("Wi-Fi password")
 y=34   textarea           560×56, 24 px font, VISIBLE by default
 y=100  [Hide]  Cancel  Done
 y=162  keyboard           600×288
```

Why this over the obvious fixes (shrink the tab, scroll the field into view):

- **The field can never be occluded**, because its position is fixed and known — it does not depend on which tab is open or how tall a list happens to be.
- **One entry point.** `LV_EVENT_CLICKED` on the field opens the editor. There is no hidden/shown state to get stuck in, so fault 2 cannot recur.
- **Explicit Done/Cancel.** Committing on close is a decision, not an accident; Cancel discards.
- **Room for a real font.** 24 px in the editor versus a cramped inline field.

**Passwords default to visible in the editor.** The entire point is to see what you are typing on a device you are holding; a "Hide" checkbox is there for anyone who wants it. The field on the settings tab itself stays masked, with its own "Show password" toggle — see [[D016 - Wi-Fi is provisioned on-device]].

This replaced the shared keyboard entirely; `kb`, `kb_event` and `ta_event` are gone.
