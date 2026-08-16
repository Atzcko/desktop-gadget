---
title: D018 - Emotion API - one engine, two transports
type: decision
id: D018
date: 2026-08-16
status: accepted
origin: brief Stage 4 + owner BLE request
tags:
  - decision
  - api
---

# D018 — One emotion engine, two transports

**Decision.** The expression logic lives in `emotion.cpp` exactly once. BLE writes and HTTP POSTs both parse the **same JSON schema** and land in the same `emotion_post()` queue.

```
BLE write  ──┐
             ├──> emotion_parse() ──> queue ──> emotion_tick() ──> LVGL
HTTP POST  ──┘        (any task)              (LVGL task only)
```

**Why a queue and not a direct call.** BLE callbacks run on the NimBLE host task; HTTP handlers run on the async TCP task. **Neither may touch an LVGL object.** This is the same rule the weather task follows, and the third time it has mattered in this project — it is now the house rule in [[CLAUDE]].

`emotion_post()` uses a zero-timeout `xQueueSend`, so a wedged UI can never stall the BLE or HTTP task; it returns "queue full" instead.

## Why Nordic UART Service

Rather than invent a profile, the BLE side uses **NUS** UUIDs (`6E4000 01/02/03…`). Every BLE tool, phone app and `bleak` tutorial already understands it, so the device is drivable from a generic scanner with no custom decoder. Write JSON to RX, get `ok <state> <seconds>` or `err <reason>` on TX.

## Validation

Bad input must be a clean rejection, never a reboot:

- malformed JSON → 400 with the parser's own message
- unknown state → 400 listing the valid states
- message > 20 chars → 400
- absent `duration_s` → **not** an error; defaults to 5
- `duration_s` clamped to 1..300, so a typo cannot hide the clock for an hour

## Visual language

Each state stays inside the existing palette — black, charcoal, white, one accent. No new type sizes: the optional message reuses the smaller weather size.

| State | Rendering |
|---|---|
| `thinking` | accent seam pulse + a ripple alternating between cards |
| `working` | steady accent pulse on the seam |
| `success` | green seam pulse ×2 + one ripple on both cards |
| `error` | fast red seam pulse ×4 |
| `celebrate` | amber pulse, bouncing round eyes, rapid ripples |
| `sleepy` | flat bar eyes, slow breathing fade |

The "ripple" is the split-flap fold run with **unchanged digits** — it reads as the card twitching rather than the clock ticking, which reuses the animation machinery from Stage 2 for free.

> [!warning] Overlay objects must call `decor()`
> Eyes and the message label are `lv_obj_create`d over the clock. Per [[D014 - Touch hit-testing]] they would otherwise swallow the hold gesture whenever an emotion was showing.
