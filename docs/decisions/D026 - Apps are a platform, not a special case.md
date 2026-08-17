---
title: D026 - Apps are a platform, not a special case
type: decision
id: D026
date: 2026-08-17
status: accepted
origin: owner
tags:
  - decision
  - architecture
---

# D026 — Apps are a platform, not a special case

**Ask.** A timer, reached by swiping up from the bottom into an app screen — and
*"restructure the code so you can add apps to it."*

The second half is the real request. A timer bolted onto `ui.cpp` would work and
would be the wrong answer, because the next app would have to be bolted on too.

## The pattern already exists — it just has no name

`ui_settings.cpp` is already an app in everything but name:

- it builds its **own LVGL screen** with `lv_obj_create(NULL)`
- it is **opened by a gesture** on the clock
- it **tears itself down** on exit and calls `lv_scr_load(ui_screen())`
- the clock loop **skips its work** while it is open

That is the whole contract, discovered by building one screen rather than
designed up front. The restructure **names and generalises** what is already
working, which is a much safer move than inventing an abstraction.

## The contract

```c
struct App {
    const char *name;
    void      (*icon)(lv_event_t *e);   /* draw its glyph in the drawer   */
    lv_obj_t *(*create)(void);          /* build and return its screen    */
    void      (*destroy)(void);         /* free everything it allocated   */
    void      (*tick)(void);            /* optional, from the LVGL loop   */
};
```

`app_host.cpp` owns the registry, the drawer, launching, and the route home.
Apps live in `src/apps/`.

### Rules an app must follow

1. **Create on entry, destroy on exit.** No app keeps a screen alive in the
   background. Settings has done this all along and it is why PSRAM has stayed
   at ~7.3 MB free with a 527 KB framebuffer and a 265 KB zoom canvas resident.
2. **Never touch the clock's objects.** The host mediates. Same discipline as
   the threading rule — the failure mode is identical and just as hard to find.
3. **`decor()` every decorative object.** LVGL sets `CLICKABLE` and
   `SCROLL_CHAIN` on everything; leaving them on breaks the host's gestures
   from inside the app. [[D014 - Touch hit-testing]]
4. **Only draw from the LVGL task.** [[D018 - Emotion API - one engine, two transports]]

## What is deliberately NOT an app

**The clock.** It is the resting state of the object, not one option among
several. Every design decision in this project has pushed the same way — true
black so the pixels are off, animations that revert on their own, a display
that clears when work stops. Demoting the clock to a tile in a grid would
contradict all of it.

So: home is the clock, the drawer is a layer above it, apps are screens the
drawer launches, and everything returns home on its own.

## Why a drawer rather than the double-tap that was first suggested

The original idea was double-tap → timer. The owner replaced it with a drawer
before any code existed, and it is the better shape: **double-tap addresses one
app, a drawer addresses all of them.** It also leaves the gesture budget intact —
see [[D027 - The gesture budget]].
