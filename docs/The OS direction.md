---
title: The OS direction
type: project
tags:
  - project
  - roadmap
updated: 2026-08-22
---

# The OS direction

The owner's question (2026-08-22): *"Is it possible to turn this into a real
OS so you add apps and do things with this?"* The honest frame: FreeRTOS is
already underneath, and the app platform ([[D026 - Apps are a platform, not a special case]])
is the part of an OS people mean. What was missing is **adding an app without
reflashing the whole image**. Three routes were assessed:

| Route | Verdict |
|---|---|
| **OTA full images** | ✅ **Done** — v1.16.0, [[D034 - Updates ship over the air]]. "Add an app" = build here, push over Wi-Fi, ~1 min. |
| **Dynamically loaded native ELF** | ❌ Rejected. No MMU → no isolation: a bad pointer in a downloaded app reboots the clock. ABI pain on every host change. All of the fragility, none of the safety. |
| **Embedded scripting runtime** | ✅ **Done** — v1.19.0, [[D037 - Apps become Lua scripts]]. Lua 5.4, apps as text files, no reboot. |

## The scripting stage — as built (v1.19.0)

Embed a small interpreter and expose the platform as its API — cards, the
digit fonts, the fold, `tick()`, `back()`, HTTP fetch, NVS. Apps become
uploadable files listed in the drawer beside native ones; the interpreter
mediates every call, so a broken script fails alone — the sandboxing the
hardware cannot provide, provided by construction.

Candidates, in current order of preference:

1. **Berry** — built for exactly this (Tasmota's app language), ~300 KB,
   embeds into an Arduino C++ firmware without ceremony.
2. **Lua** — same shape, larger ecosystem, slightly bigger.
3. **MicroPython** — official LVGL bindings but heavyweight, and grafting it
   into an existing Arduino firmware is a rebase, not an addition.

Costs known up front: the binding layer is the real work (weeks, not days);
one UI thread means scripts stay cooperative like everything else; the
`/update`-style upload endpoint and OTA are the delivery path already built.

## Standing constraints (unchanged by any of this)

- Only the LVGL task draws ([[Module map#The one rule that shapes everything]]).
- Apps create on entry, destroy on exit — nothing resident.
- No auth on the LAN API yet; a token gate is one line the day it matters
  ([[D034 - Updates ship over the air#What this deliberately does not have]]).

## Related

- [[D034 - Updates ship over the air]] · [[D035 - The Lab may only touch pins the firmware does not own]]
- [[RELEASES]]
