---
title: Stage 4 - Emotion API
type: stage
stage: 4
status: planned
tags:
  - stage
---

# Stage 4 — Emotion API

**Goal.** An HTTP endpoint that lets Claude Code animate the device during a working session.

## Scope

- ESPAsyncWebServer on port 80, mDNS `flipclock.local`.
- `POST /emotion` — `{"state": ..., "duration_s": 5, "message": "≤20 chars"}`
- `GET /health` — uptime, Wi-Fi RSSI, last weather sync.
- Auto-revert to the clock after `duration_s`.
- `CLAUDE.md` documenting the curl calls and **explicitly permitting Claude to call the endpoint unprompted**.

## State → animation

| State | Animation |
|---|---|
| `thinking` | slow ripple-flip travelling across the cards |
| `working` | steady accent pulse along the card seam |
| `success` | single bright flip, accent flash |
| `error` | sharp double pulse on the seam |
| `celebrate` | rapid ripple-flip + emotive eyes overlay |
| `sleepy` | eyes overlay, drooping; brightness dips for the duration |

All of it stays inside the existing aesthetic — black, charcoal, white, one accent. No new type sizes; the optional message renders in the smaller weather size beneath the clock.

> [!warning] Async handler → LVGL is a threading boundary
> ESPAsyncWebServer callbacks run on the async TCP task. They must **not** touch LVGL objects. The handler validates, parses, and posts a request to a queue; the LVGL loop picks it up. Same discipline as the weather task in [[Stage 3 - Weather]].

## Validation

Malformed JSON, unknown state, absent `duration_s`, and an over-length message must all produce a clean 4xx rather than a reboot. `duration_s` gets clamped to a sane ceiling so a typo cannot hide the clock for an hour.

## Acceptance

- [ ] `curl -X POST http://flipclock.local/emotion -H "Content-Type: application/json" -d '{"state":"celebrate","duration_s":5}'` animates, then reverts
- [ ] `flipclock.local` resolves from the Mac
- [ ] `GET /health` returns uptime, RSSI, last weather sync
- [ ] Bad payloads return 4xx and do not crash
- [ ] Clock keeps correct time throughout

## Next

[[Stage 5 - Touch and burn-in guard]]
