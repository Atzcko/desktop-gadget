---
title: Stage 1 - Static digits
type: stage
stage: 1
status: awaiting-hardware-verification
date_started: 2026-08-16
tags:
  - stage
---

# Stage 1 — Static digits

**Goal.** Our own project builds and renders the Fliqlo layout with a hardcoded time. No Wi-Fi, no NTP, no animation.

## Scope

- Project skeleton: `platformio.ini` (per [[D003 - Copy the library platformio env verbatim]]), `boards/T-Display-AMOLED.json` copied in, `include/lv_conf.h`.
- `config.example.h` complete; `config.h` gitignored and created from it.
- `amoled.beginAMOLED_241(true, false)` → `beginLvglHelper(amoled)` per [[D009 - LVGL buffer strategy]].
- Pure black background, two rounded charcoal cards, horizontal centre seam, huge white `HH : MM`.
- Weather block laid out with placeholder values so the composition can be judged.
- Brightness set to `BRIGHTNESS_DAY`.

## Design targets

Derived from `amoled.width()` / `amoled.height()`, never hardcoded — see [[D004 - Rotation 0 is already landscape]].

| Element | Intent |
|---|---|
| Background | `#000000` — AMOLED pixels genuinely off, not near-black |
| Card fill | dark charcoal, ~`#171717` |
| Card corners | large radius |
| Seam | ~~a 2 px pure-black line across each card's vertical centre~~ removed in v1.4.1 — see [[D012 - Card geometry]] |
| Digits | white, the largest font in the build |
| Colon | between the two cards, dimmer than the digits |

> [!success] Resolved — a generated font
> Montserrat tops out at 48 px and LVGL 8 labels cannot be scaled. Settled by generating a real 210 px, digits-only LVGL font with `lv_font_conv`: ~90 KB, tabular advance of 116.8 px per digit. All card geometry derives from that number. See [[D011 - Generate the digit font, do not scale Montserrat]] and [[D012 - Card geometry]].

## Outcome — 2026-08-16

**Build: SUCCESS** first attempt, 18.9 s.

```
RAM:   [=         ]   6.8% (used 22140 bytes from 327680 bytes)
Flash: [=         ]  12.7% (used 834041 bytes from 6553600 bytes)
```

Note the contrast with Stage 0's 2.5 MB — the Factory example links Wi-Fi, BLE, SD and the full sensor suite. Our Stage 1 image is a third of that. Wi-Fi lands in Stage 2 and will grow it substantially.

**Flash: SUCCESS**, 834 400 B at 919 kbit/s, hash verified.

**Serial:**

```
=== Desktop gadget — Stage 1 (static digits) ===
Board      : 2.41 inch
Panel      : 600 x 450
Touch      : online
PSRAM free : 8386051 bytes
Heap free  : 358184 bytes
PSRAM after LVGL : 7841411 bytes
Ready in 1397 ms
```

### Three predictions confirmed on hardware

| Claim | Evidence |
|---|---|
| [[D004 - Rotation 0 is already landscape]] — 600×450 with no `setRotation` | `Panel : 600 x 450`, and no rotation call exists in `main.cpp` |
| [[D009 - LVGL buffer strategy]] — one ~527 KB PSRAM buffer | PSRAM dropped **544 640 B** across `beginLvglHelper()`; predicted 540 000 B + allocator overhead |
| Touch controller present | `Touch : online`, i.e. `hasTouch()` true |

**Boot time 1397 ms** against a 10 s budget. Wi-Fi association and the first NTP exchange will dominate Stage 2's number, but the headroom is large.

## Acceptance

- [x] Builds clean against our own `platformio.ini`
- [x] Boots without PSRAM allocation failure
- [x] Panel reports 600×450 landscape
- [ ] Renders a static `12 : 34` — **needs your eyes**
- [ ] Background is true black, indistinguishable from the bezel in a dark room
- [ ] Digits legible at desk distance, card proportions look right

## Next

[[Stage 2 - NTP and flip animation]]
