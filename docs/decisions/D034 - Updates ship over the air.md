---
title: D034 - Updates ship over the air
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - infrastructure
  - api
---

# D034 — Updates ship over the air

## Context

Every release so far crossed a USB cable. That has cost real time twice — the
port renamed itself mid-session (`usbmodem2101` → `usbmodem1101`) and broke an
upload with a bare `Error 2` — and it makes the device untouchable when the
Mac is not physically next to it. The owner asked for OTA, as the first step
of the larger "real OS" direction: apps cannot be added remotely while the
image cannot be replaced remotely.

**The enabling fact, checked rather than assumed:** the board definition has
used `default_16MB.csv` since Stage 0, and that table already contains
`ota_0` + `ota_1` (6.5 MB each) and `otadata`. The second slot was always
there. No repartition, therefore no NVS wipe, therefore this is a MINOR
release — not the v2.0.0 the plan had budgeted.

## Decision

**`POST /update` on the existing async web server takes a raw firmware image
and writes it to the inactive slot.**

- **Raw body, not multipart.** The client is `curl` in a script, not a
  browser. `Update.begin(total)` gets the exact size up front, and the first
  write validates the ESP32 image magic — pushing a wrong file fails in the
  first chunk, not after 1.7 MB.
- **Reboot on disconnect, not in the handler.** The 200 response must reach
  the client before the restart; hooking `onDisconnect` makes TCP itself the
  confirmation that it did.
- **The device narrates its own update.** The upload posts `flashing` to the
  emotion queue at start — the same thread-safe path every other transport
  uses — and `error` if it fails. The line on the desk shows the update the
  way it shows everything else.
- **`tools/ota` is the release path**: build, discover the device (cached IP,
  then mDNS), push, poll `/health` until the new version answers. The script
  refuses to declare success until the device itself reports the new version
  — the same trust-the-device rule the release checklist already has.

## What this deliberately does not have

- **No authentication.** Anyone on the LAN can flash the clock. Accepted for
  a desk gadget on a private network; the hook (an `X-OTA-Token` header
  checked against an NVS field) is trivial to add the day it matters.
- **No rollback.** The stock Arduino core does not enable
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, so an image that boots and then
  crashes will crash-loop until rescued. **The cable remains the rescue
  path** — OTA replaces routine flashing, not recovery. Mitigation: `pio run`
  must succeed locally before `tools/ota` will push, and the magic-byte check
  rejects non-firmware files.
- **No resume.** A failed push is simply pushed again; `Update.abort()`
  resets state so a retry starts clean.

## Consequences

- A release is now: bump, document, commit, tag, `tools/ota`, confirm
  `/health`. No cable, no port detection, no boot mode.
- LVGL may stutter briefly while flash sectors erase under the animation.
  Cosmetic, seconds long, accepted.
- The image must stay under 6.5 MB. It is 1.74 MB; not a concern this year.
- First release with the endpoint (v1.16.0) still goes over the cable — the
  running firmware has no `/update` yet. Its successor proves the path.

## Related

- [[D003 - Copy the library platformio env verbatim]] — where the partition
  table came from
- [[D018 - Emotion API - one engine, two transports]] — the queue the OTA
  handler posts through
