---
title: D039 - The device must be drivable without a finger
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - infrastructure
  - api
---

# D039 — The device must be drivable without a finger

## Context

Every screen on this device could only be reached by touching it. I cannot
touch it. So every UI change all session ended the same way: build it, ship it,
and hand the owner a checklist — the acceptance lists in the stage notes exist
because verification had to be delegated.

That was tolerable while the bugs were cosmetic. It stopped being tolerable
when launching a Lua app rebooted the device: I could read the code and guess,
but I could not reproduce, and [[D038 - Crashes must be readable without a cable]]
had just made the *result* of a crash readable while leaving the *trigger*
out of reach.

## Decision

**`POST /launch {"name":"Blink"}` opens any screen.** `clock` routes home,
`drawer` opens the drawer, any app name launches that app. `GET /health`
reports the current screen.

The request is recorded by the web server's task and **served on the LVGL
loop** — `app_host_tick()` consumes it. Touching LVGL from the async task is
the one rule this firmware has never broken
([[D018 - Emotion API - one engine, two transports]]) and this does not break
it either.

## What it changed immediately

The reboot on launching a script app went from "four plausible causes, ranked
by guesswork" to a reproduction in one command:

```
uptime 37 s → POST /launch {"name":"Blink"} → uptime 5 s
```

and, because the crash dump then came from the running build,
`tools/crash` decoded it to `script.cpp:343` on the first try
([[D040 - A script must not be able to reboot the clock]]).

## Consequences

- Acceptance items that are about *reaching* a screen can now be checked here.
  Ones about touch **feel** — a drag, a fold, a tap target — still cannot, and
  the stage notes keep those.
- Anyone on the LAN can change what the device is showing. Same posture as the
  rest of the API ([[D034 - Updates ship over the air]]): a desk gadget on a
  private network, with the token hook noted for the day that changes.
- It is a genuine feature as well as a test hook — a script or a shortcut on
  the Mac can now put the clock on a particular screen.

## Related

- [[D038 - Crashes must be readable without a cable]] — reading the result;
  this is reaching the trigger
- [[D026 - Apps are a platform, not a special case]]
