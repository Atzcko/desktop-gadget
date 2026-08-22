---
title: D037 - Apps become Lua scripts
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - architecture
  - apps
---

# D037 — Apps become Lua scripts

## Context

[[The OS direction]] assessed three routes to "add apps without reflashing".
OTA ([[D034 - Updates ship over the air]]) closed most of the gap: a new app
is a build plus a one-minute push. The owner asked for the rest — **add and
remove apps without restarting** — with the core staying C++.

The full-interpreter option was considered and rejected in the same
conversation: the RM690B0 QSPI panel and CST226SE touch drivers are the least
portable part of the system, the per-frame line and fold maths wants to be
native, and ~5300 lines of hardware-verified C++ would be re-opened along with
every bug the decision notes record.

## Decision

**A hybrid. The core stays C++; the app layer gains a Lua 5.4 runtime.**

A script app is `/apps/<name>.lua` in LittleFS implementing the same lifecycle
the native contract does — `on_create`, `on_tick`, `on_back`, `on_exit` — as
globals. It appears in the drawer beside Timer, Settings and the Lab, and
nothing about launching it differs.

### Lua, and vendored

Lua 5.4.7 copied into `lib/lua/` verbatim, minus `lua.c`/`luac.c`. Not from
the PlatformIO registry: those entries are third-party wrappers of varying
maintenance, while Lua's own source is dependency-free C that has compiled
everywhere for twenty years. Vendoring makes the version a fact of the repo
rather than a resolution result. Cost measured, not guessed: **+148 KB flash
(27.3 % → 30.0 %), +1 KB RAM.**

### A fresh `lua_State` per launch

Created in `create()`, closed in `destroy()`. "Create on entry, destroy on
exit" then holds for scripts exactly as for native apps, and nothing can leak
from one run into the next.

### Lua allocates from PSRAM, never the internal heap

There is 8 MB of PSRAM against ~60 KB of internal heap in use. A script that
leaks must not be able to starve the display driver, which allocates
internally.

### An instruction budget, which is the whole safety story

Scripts run on the LVGL task — that is *why* they may touch LVGL safely
([[Module map#The one rule that shapes everything]]). It also means
`while true do end` would freeze the clock with the cable as the only way out,
which is precisely the failure this feature must not have. A `LUA_MASKCOUNT`
hook stops any entry point past 400 k VM instructions and reports it on
screen. Generous for building a screen, absurd for a tick.

### Compile at upload, not at launch

`script_save()` parses into a throwaway state before writing. A broken script
is rejected with the parser's own message and the previous working version of
that name is untouched — rather than becoming a tile that fails when tapped.

### A whitelist, not the language's own library set

`io`, `os`, `package` and `debug` are **not** loaded. A UI script has no
business opening files, spawning processes or loading C modules, and each is a
way out of the sandbox the interpreter otherwise provides. GPIO bindings
enforce the same pin rules as the Lab, including GPIO0 being readable and
never drivable ([[D035 - The Lab may only touch pins the firmware does not own]],
[[D036 - GPIO0 is readable, never drivable]]).

Names are validated to letters, digits, space, `-` and `_` — no dot, no slash,
so a name cannot escape `/apps/`. Verified against `?name=../../etc/passwd`.

## The registry stopped being an array

`APPS[]`/`APP_COUNT` became `app_count()`/`app_at()`, native entries first so
a native app never shifts because a script appeared. `settings.app_order` holds
indices into that list, and a stale order is now normal rather than
corruption — so `order_valid()` checks the saved array is a permutation of
0..n-1 and falls back to identity for the **whole** list if not. Trusting half
of it would hide an app.

**No reboot is needed to see a new app**, and that falls out of an earlier
decision rather than new work: the drawer is rebuilt on every visit
([[D033 - Back goes one level, not home]]), so it re-reads the registry each
time it opens.

> [!warning] `text/plain` uploads arrive empty
> ESPAsyncWebServer special-cases `Content-Type: text/plain`: if the body
> starts with param-like characters and contains `=`, it parses the whole
> thing as form data and **never calls the body handler**. Lua source is full
> of `=`. `tools/app` sends `application/octet-stream`, as OTA already did.
> The symptom is an empty upload with no error anywhere.

## What this deliberately does not have

- **No I²C or UART bindings yet.** The Lab covers those natively; adding them
  to the script API is a later, small step.
- **No auth**, consistent with `/update` — private LAN, token hook noted.
- **No preemption.** The budget stops a runaway between entry points, not
  mid-C-call. A binding that blocks still blocks.

## Consequences

- Adding an app is `tools/app add foo.lua` — under a second, no reboot, the
  clock never stops. Verified live on 2026-08-22, including delete and re-add.
- Two example apps ship in `apps/`: `blink.lua` (GPIO, tick, buttons) and
  `uptime.lua` (cards, the clock's digit font).
- Scripts are slower than C++. Anything doing per-frame maths should still be
  a native app; the runtime is for the long tail.
- The next honest step is the Lab's I²C/UART surface exposed to scripts, which
  turns the bench tool from a fixture into an instrument.

## Related

- [[The OS direction]] · [[D026 - Apps are a platform, not a special case]]
- [[D034 - Updates ship over the air]] — the delivery path this rides on
