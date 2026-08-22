---
title: D040 - A script must not be able to reboot the clock
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - bug
  - apps
---

# D040 — A script must not be able to reboot the clock

## Context

Tapping either Lua app rebooted the device.
[[D037 - Apps become Lua scripts]] had promised the opposite in as many words:
*"the interpreter mediates every call, so a broken script fails alone — the
sandboxing the hardware cannot provide, provided by construction."* That claim
was false, and two independent defects made it false.

## Defect 1 — `-DLUA_USE_C89=0` turned C89 mode ON

`lib/lua/library.json` carried `"flags": ["-DLUA_32BITS=0", "-DLUA_USE_C89=0"]`,
added to be explicit about the configuration. But `luaconf.h` tests the two
macros differently:

```c
#define LUA_32BITS 0
#if LUA_32BITS              /* value-tested — -D...=0 is correctly OFF */

#if defined(LUA_USE_C89)    /* EXISTENCE-tested — -D...=0 turns it ON  */
```

So the flag intended to disable C89 mode enabled it — **for the library only**,
since `library.json` build flags do not reach the consumer. Inside `lib/lua`,
`lua_Integer` became `long` (4 bytes); in `src/script.cpp` it stayed
`long long` (8). `LUAL_NUMSIZES` therefore disagreed, and
`luaL_checkversion` — reached through `luaL_newlib` inside `register_api` —
correctly reported *"core and library have different numeric types"*.

Both flags are removed. `luaconf.h`'s own defaults apply on both sides, which
is what "be explicit" should have meant.

> [!warning] `-DFOO=0` does not disable a `defined()`-guarded macro
> It enables it. Check which test a header uses before passing a flag to
> "make the default explicit" — and remember that `library.json` flags compile
> the library, not its consumers, so a mismatch is silent until something
> checks.

## Defect 2 — an unprotected Lua error called `abort()`

The version error was raised inside `luaL_requiref`, which `script_create`
calls **directly, not under `lua_pcall`**. Lua's `luaD_throw` with no error
handler and no panic function does exactly one thing: `abort()`. On an ESP32
that is a panic and a reboot.

So the crash was not "a script did something bad". It was **a Lua API error
of any kind, anywhere outside a pcall, taking down the device** — including
every future one.

`lua_atpanic` now installs a handler that records the message and `longjmp`s
back into `script_create` (a panic function must not return; if it does, Lua
aborts anyway). The whole init sequence — `requiref`, `register_api`,
`loadbuffer`, `on_create` — runs inside that guard. On a panic the message is
shown on the app's own screen and the state is closed, since a panicked
`lua_State` is unusable for anything else.

## Consequences

- D037's promise is now true for the paths that had escaped it. Errors *inside*
  script code were always protected — `call_global` and `button_cb` use
  `lua_pcall` — but the runtime's own setup was not.
- A script with a syntax or runtime error shows a message and leaves the clock
  running, which is what the drawer needed to be safe.
- Found by reproducing remotely ([[D039 - The device must be drivable without a finger]])
  and decoding a same-build core dump
  ([[D038 - Crashes must be readable without a cable]]) — the two tools built
  that morning, used in anger the same afternoon.

## Related

- [[D037 - Apps become Lua scripts]] — the promise this restores
