---
title: D038 - Crashes must be readable without a cable
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - infrastructure
  - bug
---

# D038 — Crashes must be readable without a cable

## Context

The owner reported: *"The device restarts if you want to scroll in the app
page."* The cable had been unplugged the same day, on my own advice, because
OTA made it unnecessary for normal work ([[D034 - Updates ship over the air]]).

Which meant no serial log, no backtrace, and no way to reproduce — I cannot
drive touch. The honest position was: I could not tell which of several
plausible bugs it was.

**That is the real defect.** A device that ships its own updates and cannot
report its own crashes has traded away the only tool that turns "it restarts"
into a fix.

## Decision

**`GET /crash` reports the last panic over Wi-Fi.**

Nothing needed enabling. The partition table has carried a `coredump`
partition since Stage 0 ([[D003 - Copy the library platformio env verbatim]]),
and arduino-esp32 ships `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` with ELF
format — the panic handler has been writing full core dumps to flash for the
project's entire life. They needed **reading**, not enabling.

The endpoint returns `esp_reset_reason()` plus, when a dump exists, the
faulting task, `exc_pc`, exception cause, faulting address and a 16-deep
backtrace. `tools/crash` fetches it and pipes the addresses through
`xtensa-esp32s3-elf-addr2line`, so a crash becomes file-and-line.

`DELETE /crash` erases the stored dump, so the next one is unambiguous.

> [!warning] Decode against the ELF that crashed
> `addr2line` is only truthful against the exact build that produced the
> dump. Rebuild after a crash and the line numbers are fiction.
>
> **This is enforced, not just documented** (v1.19.2). The dump carries the
> first 16 hex chars of the crashing image's ELF sha256; `/crash` compares it
> with the running image's and reports `same_build`, and `tools/crash` refuses
> to decode a mismatch. The first real capture proved why: it decoded into a
> stack mixing `build_drawer` with `l_ui_label`, functions that never call
> each other. The reset reason and IDF-region frames stay trustworthy
> regardless, because those addresses do not move between builds.

## The bug this was built to find, found by reading instead

While building it, one crash path turned out to be provable from source:

**LVGL's `obj_del_core()` clears `act_obj`, `last_obj` and `last_pressed`
when an object is deleted — but never `scroll_obj`.** Delete a screen while a
finger is mid-scroll on it and the input device keeps a pointer into freed
memory, which the scroll-throw handler dereferences on the next read. The
reboot lands a fraction of a second after the screen changed, with nothing to
connect it to scrolling.

Fix: `lv_indev_reset(NULL, NULL)` before every screen deletion.
`indev_proc_reset_query_handler()` runs before any further processing and
nulls `scroll_obj` along with the rest.

This is now a rule, not a patch:

> [!warning] Release the input device before deleting a screen
> `app_host_home()`, `app_host_back()` and `app_host_launch()` all call
> `release_input()` first. Any future code that deletes a screen must too.
> [[D031 - The layout is the model]] said load the new screen before deleting
> the old one; this is the other half of the same rule.

## What else was wrong in that code path

Three defects the report led to, each real regardless of which caused the
reboot:

1. **The drawer could not show five apps.** One flex row: 5 × 118 + 4 × 22 =
   678 px of tiles in a 600 px row, overflow drawn off-screen and unreachable.
   *That is why the owner was trying to scroll.* It now wraps and scrolls
   vertically — vertically only, so it cannot fight the left-edge back
   gesture for the same finger movement.
2. **A drag in the drawer launched an app.** `LV_EVENT_CLICKED` fires on any
   press-release over a tile. The clock has disqualified moved presses since
   [[D027 - The gesture budget]]; the drawer never did. It does now, and the
   same flag suppresses a reorder that was really a scroll.
3. **Script uploads rebuilt the registry from the web server's task**, racing
   every `app_at()` the LVGL task makes — and, if the drawer was open,
   invalidating the `App` pointers its tiles carry as event user data. Uploads
   now only mark the registry dirty; the rebuild happens on the LVGL task and
   only while nothing is open ([[D037 - Apps become Lua scripts]]).

## Consequences

- A crash is now a URL, and the cable stays unplugged.
- `/apps` gained a `pending` flag; a newly uploaded app becomes visible once
  you are back at the clock, which is where you already are when uploading.
- **Residual, and stated rather than hidden:** LittleFS writes still happen on
  the web server's task. An upload landing at the exact moment a script app is
  launching is an unguarded overlap. Rare, and the fix is to move the write
  behind the same deferral — worth doing when scripts get I²C and UART.

## Related

- [[D034 - Updates ship over the air]] — why the cable came out
- [[D031 - The layout is the model]] · [[D027 - The gesture budget]]
