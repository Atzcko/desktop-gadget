---
title: D031 - The layout is the model
type: decision
status: accepted
date: 2026-08-18
tags:
  - decision
  - apps
  - bug
---

# D031 — The layout is the model

## Context

Long-pressing a drawer tile to reorder it **reset the device, every time.**

The handler saved the new order, deleted the drawer, and called
`app_host_open_drawer()` to rebuild it. Reasonable-looking code, and wrong in
two ways at once:

1. **It deleted the active screen from inside an event callback on one of that
   screen's own grandchildren.** `lv_disp_t.act_scr` is left dangling, and the
   very next `lv_obj_create()` in the rebuild walks up the tree to invalidate
   through it. The reset was not a coincidence of timing; it was structural.
2. **It would then have launched the app it had just moved.** LVGL sends
   `LV_EVENT_CLICKED` on *every* release, including the end of a long press —
   `LV_EVENT_SHORT_CLICKED` is the one that fires only for short ones. So the
   reorder was always going to be followed by a launch.

## Decision

**Nothing gets deleted.** The tiles are flex children, and a flex container
lays out in child order — so moving one child is the reorder:

```c
lv_obj_move_to_index(cell, prev);
```

The layout *is* the model. There was never a reason to rebuild a screen to
change the order of things already on it.

And two rules that generalise past this bug:

> [!warning] Never delete a screen from inside an event on that screen
> If a handler genuinely must replace the screen it is running in, load the new
> one first and defer the delete — which is what `app_host_home()` already does,
> and why *it* has never crashed.

> [!warning] `LV_EVENT_CLICKED` fires after a long press too
> A long-press handler and a click handler on the same object will both run.
> Use `LV_EVENT_SHORT_CLICKED` for the click, or suppress it with a flag
> cleared on the next `LV_EVENT_PRESSED`. The drawer does the latter, because
> the flag also documents why.

## Consequences

- Reordering is instant and allocates nothing.
- One flag (`reorder_fired`) and one `PRESSED` handler on each tile.
- The same `SHORT_CLICKED` trap exists anywhere else long-press and tap share
  an object — the timer's cards hit it too, and use `SHORT_CLICKED`
  ([[D032 - Three gestures, one control]]).

## Related

- [[D026 - Apps are a platform, not a special case]]
- [[D014 - Touch hit-testing]]
