---
title: App platform
type: stage
status: awaiting-hardware-verification
date_started: 2026-08-17
tags:
  - stage
  - architecture
---

# App platform — drawer, and the first app

Design in [[D026 - Apps are a platform, not a special case]] and
[[D027 - The gesture budget]]. **Documented before implementation**, at the
owner's instruction, because the restructure is the substance of the request and
a timer bolted onto `ui.cpp` would have satisfied the letter of it and none of
the point.

## Scope

1. `app_api.h` — the `App` contract.
2. `app_host.cpp` — registry, the drawer screen, launch, and the route home.
3. **Swipe up from the bottom edge** on the clock opens the drawer, without
   disturbing tap, long-press or hold.
4. `apps/app_timer.cpp` — the first app.
5. `ui_settings.cpp` stays as it is for now. It already follows the contract; it
   can be adopted into the registry later, and doing both at once would mean a
   restructure and a new feature landing in the same untested step.
   **Done in v1.12.0** — `apps/app_settings.cpp` is a thin adapter, and the
   3-second hold still opens Settings directly. Apps are reorderable by
   long-pressing a tile; the order persists in NVS.

## Timer app

| | |
|---|---|
| Set | drag each card — minutes card sets minutes, seconds card sets seconds ([[D028 - Set a number by dragging the number]]) |
| Digits | the existing card language — `MM : SS`, same fonts, and they fold |
| Controls | play / pause · reset · back |
| Finish | the line, so the alert reuses the emotion renderer rather than inventing one |

## Acceptance

- [ ] Swipe up from the bottom opens the drawer; from the middle does nothing
- [ ] Tap still shows date + sync age
- [ ] Long press still cycles brightness
- [ ] Hold 3 s still opens Settings
- [ ] **A swipe never fires a tap or a brightness change on the way**
- [ ] Timer counts down, pauses, resumes, resets
- [ ] Back returns to the clock; the clock is still correct and still ticking
- [ ] PSRAM returns to its previous level after leaving the app
- [ ] Settings appears in the drawer, and the 3 s hold still opens it directly
- [ ] Long-pressing a drawer tile moves it left; the order survives a power cycle
- [ ] Dragging the minutes card changes only minutes; the seconds card only seconds
- [ ] Digits fold on every change; a fast drag does not tear
- [ ] Dragging back to the start restores the number you started with
