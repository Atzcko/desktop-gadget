---
title: D041 - Nothing may unwind through a live interpreter
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - bug
  - apps
---

# D041 — Nothing may unwind through a live interpreter

## Context

Tapping **Back** inside a Lua app rebooted the device. The binding looked
harmless:

```c
static int l_back(lua_State *Ls) { app_host_back(); return 0; }
```

Follow it through. `back()` is called from Lua, so this C function runs
**inside `lua_pcall` on state `L`**. `app_host_back()` calls
`running->destroy()`, which is `script_destroy()`, which calls
`lua_close(L)` — and then `lua_pcall` returns into a state that no longer
exists.

The interpreter was being closed by code it was running.

## Decision

**A binding may request a lifecycle change. It may never perform one.**

`l_back()` calls `app_host_request_back()`, which sets a flag. The host serves
it from `app_host_tick()` on the next pass through the LVGL loop, by which
time `lua_pcall` has returned and the Lua call stack is empty.

This is the same deferral `POST /launch` already used for a different reason
([[D039 - The device must be drivable without a finger]]) — that one crosses a
task boundary, this one crosses a **call-stack** boundary. One mechanism now
serves both, in `serve_pending()`.

> [!warning] The rule, for any binding added later
> Anything that destroys the app — back, exit, launching another app — must go
> through `app_host_request_*`. A binding that touches the app's own lifetime
> synchronously is closing the interpreter that called it.

Native apps do **not** need this: their back buttons are plain C++ callbacks
with no interpreter to invalidate, and LVGL already handles deleting an object
mid-event via `_lv_event_mark_deleted()`.

## Two layout bugs found in the same report

The owner also asked *"what is that last app that is blank?"*. Two causes,
stacked:

1. **The drawer's wrapping row was 300 px against a 330 px need.** A cell is
   `TILE + 40` = 158, so two rows plus a gutter need 330; the second row was
   clipped by 30 px. The name label sits in the bottom 20 px of a cell, so the
   fifth app rendered as a square with **no name at all**. The 300 was a round
   number I picked when adding wrapping in v1.19.1 instead of computing the
   space available. It is now 332 — the entire gap between the title and the
   Clock button.
2. **Script apps have no icon.** The `App` contract allows `icon` to be null
   and nothing in a `.lua` file can draw one, so every script tile was an empty
   charcoal square. Icon-less apps now show the first letter of their name,
   which is what distinguishes one script from another at a glance.

## Consequences

- `back()` works from a script, and so will any future lifecycle binding that
  follows the rule.
- `GET /health` reports `current`, the screen now showing — promised by D039
  and, until this release, not actually wired up.
- The demo scripts are down to one. `uptime.lua` is deleted; `blink.lua`
  became a real bench tool that selects any output-capable pin on the device,
  releasing the previous pin on every change.

## Related

- [[D039 - The device must be drivable without a finger]] — the same deferral,
  across a task boundary
- [[D040 - A script must not be able to reboot the clock]] — the other half of
  making the sandbox true
- [[D037 - Apps become Lua scripts]]
