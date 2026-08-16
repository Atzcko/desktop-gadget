---
title: Stage 1 - Static digits
type: stage
stage: 1
status: planned
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
| Seam | a 2 px pure-black line across each card's vertical centre |
| Digits | white, the largest font in the build |
| Colon | between the two cards, dimmer than the digits |

> [!tip] Font size is the real constraint
> LVGL 8 fonts are compiled in at fixed sizes via `LV_FONT_MONTSERRAT_*` in `lv_conf.h`, and the built-in Montserrat set tops out well below the ~200 px a 450 px-tall panel wants for Fliqlo digits. Options: enable the largest available and scale with `lv_img`/transform, or generate a custom font. This is the open question of Stage 1 and is settled on hardware, by eye.

## Acceptance

- [ ] Builds clean against our own `platformio.ini`
- [ ] Boots to a static `12 : 34`
- [ ] Background is true black — indistinguishable from the bezel in a dark room
- [ ] Digits legible at desk distance
- [ ] No PSRAM allocation failure

## Next

[[Stage 2 - NTP and flip animation]]
