---
title: D008 - Brightness scale
type: decision
id: D008
date: 2026-08-16
status: accepted
origin: brief
tags:
  - decision
  - amoled
---

# D008 — Brightness scale and the ~35 % default

**API.** `setBrightness(uint8_t level)` writes a single byte as the parameter of `LCD_CMD_BRIGHTNESS` straight to the panel. The usable range is therefore the **full `0..255`**, and `0` is genuinely off. The library's own `AMOLED_DEFAULT_BRIGHTNESS` is `175` (≈ 69 %).

**Decision.** Config defaults:

| Setting | Value | ≈ % of max |
|---|---|---|
| `BRIGHTNESS_DAY` | `90` | 35 % |
| `BRIGHTNESS_NIGHT` | `25` | 10 % |
| `NIGHT_START_HOUR` | `22` | — |
| `NIGHT_END_HOUR` | `7` | — |

`90/255 = 35.3 %`, which is the brief's "~35 % of max".

**Caveat worth knowing.** Panel brightness registers are rarely linear in perceived luminance, so 35 % of the register range is not 35 % of the perceived brightness — it will look brighter than a third. These are starting values to be tuned by eye on hardware, which is exactly why they live in `config.h` and are reachable from the long-press cycle in Stage 5.

**Long-press cycle** steps through `{25, 60, 90, 140, 200}` and then wraps, so the day default sits in the middle of the cycle.

**Related.** [[D010 - Night dimming is a schedule, not a sensor]]
