---
title: D014 - Touch hit-testing
type: decision
id: D014
date: 2026-08-16
status: accepted
origin: caught in review before flashing
tags:
  - decision
  - lvgl
  - touch
---

# D014 — Decorative objects must give up `LV_OBJ_FLAG_CLICKABLE`

**The bug, caught before it reached hardware.** The 5-second hold is handled by one event callback on the layout root. That only works if the press actually reaches the root.

Reading `lv_obj_constructor` in the pinned LVGL 8.4.0:

```c
obj->flags = LV_OBJ_FLAG_CLICKABLE;
obj->flags |= LV_OBJ_FLAG_SNAPPABLE;
if(parent) obj->flags |= LV_OBJ_FLAG_PRESS_LOCK;
...
if(parent) obj->flags |= LV_OBJ_FLAG_GESTURE_BUBBLE;
```

Two facts combine badly:

1. **Every** `lv_obj_create()` is CLICKABLE — unconditionally, first line of the constructor.
2. `LV_OBJ_FLAG_EVENT_BUBBLE` is **not** in that list. `GESTURE_BUBBLE` is present, but that only bubbles gestures, not press/release events.

LVGL hit-tests the deepest clickable object first. So the cards, seams, colon dots, weather row, stale dot and hold bar would each have swallowed the press, and **the 5-second hold would only have worked in the 14 px margins beside the cards** — a bug that reads as "the hold gesture is flaky" and is miserable to diagnose by hand.

**Decision.** A single helper strips the flag from anything purely visual:

```cpp
static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
```

Applied at all 9 decorative call sites in `ui.cpp`; the layout root alone re-adds `CLICKABLE`.

## The sibling bug — found on hardware, 2026-08-16

The owner reported that the clock **could be dragged around with a finger**. Same root cause, different flag. From the same constructor:

```c
if(parent) obj->flags |= LV_OBJ_FLAG_SCROLL_CHAIN;
obj->flags |= LV_OBJ_FLAG_SCROLLABLE;
```

`decor()` cleared `SCROLLABLE` but not `SCROLL_CHAIN`, and — the real miss — **`lv_scr_act()` was never touched at all**. The screen is an ordinary `lv_obj`, so it is scrollable by default. A drag on the clock was not handled by the root, got forwarded up the chain, and the *screen* scrolled, taking the entire layout with it.

**Fix, both halves:**

```cpp
lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);          /* in decor() */

lv_obj_clear_flag(scr_clock, LV_OBJ_FLAG_SCROLLABLE);    /* in ui_init() */
lv_obj_set_scrollbar_mode(scr_clock, LV_SCROLLBAR_MODE_OFF);
```

> [!warning] Do not forget the screen object
> It is easy to think of `lv_scr_act()` as a backdrop rather than an object. It is an object, with every default flag, and it is the last stop on the scroll chain. Nothing in this firmware is ever meant to scroll on the clock screen — the only thing that legitimately moves the layout is the burn-in walk, which sets a position directly rather than scrolling.

> [!tip] The rule going forward
> In this codebase, **`decor()` is the default** for any `lv_obj_create()` that exists to be looked at. Reach for a bare `lv_obj_create` + `lv_obj_remove_style_all` only when the object is genuinely meant to receive input. The Stage 4 emotion overlays must follow the same rule or they will break the gesture the moment they appear.
