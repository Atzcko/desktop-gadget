---
title: D012 - Card geometry
type: decision
id: D012
date: 2026-08-16
status: accepted
origin: Stage 1
tags:
  - decision
  - layout
---

# D012 — Card geometry

All numbers below fall out of the one font metric from [[D011 - Generate the digit font, do not scale Montserrat]]: **116.8 px per digit**, so a two-digit card needs **233.6 px** of glyph width.

| Constant | Value | Why |
|---|---|---|
| `CARD_W` | 268 | 233.6 glyph + ~17 px padding per side |
| `CARD_H` | 232 | digits are 153 px → fill ~66 % of card height, Fliqlo's proportion |
| `CARD_GAP` | 36 | colon zone |
| `CARD_RADIUS` | 26 | large, per the brief's "large rounded cards" |
| ~~`SEAM_H`~~ | ~~3~~ | **removed in v1.4.1** — see below |
| `COLON_DOT` | 14 | dots at 1/3 and 2/3 card height, straddling the seam |

Clock row total width = `268 × 2 + 36 = 572`, leaving **14 px margin** each side of the 600 px panel — comfortably more than the ±2 px the Stage 5 burn-in walk needs.

**Every constant is even.** LVGL's `lv_rounder_cb` (installed by `beginLvglHelper`) snaps flush areas to even coordinates for the RM690B0. Even geometry means a redraw region is never silently enlarged, and the burn-in walk can step in even increments without fighting it. See [[D009 - LVGL buffer strategy]].

## Two details that are easy to get backwards

> [!warning] The seam is gone as of v1.4.1
> It drew *over* the numerals, correctly, for as long as it existed — in Fliqlo the split line crosses the glyphs because it is the gap between two physical flaps.
>
> It was removed once the same card language was applied at 55 px and 58 px as well as 232 px. A 2 px black line across a 55 px card has too few pixels to sit cleanly and reads as choppy. **The fold animation still hinges at `CARD_H/2`** — the seam was only the drawn hint of where that hinge is, and the fold shows it better.

> [!tip] `clip_corner` on the card
> Set now, needed later: when the Stage 2 fold animation moves a digit copy past the card edge, clipping keeps it inside the rounded rectangle instead of spilling onto the black background.

## Positions

- Clock row `y = 44`, height 232 → occupies y 44..276.
- Weather row aligned `TOP_MID` at `y = 44 + 232 + 26 = 302`.
- Weather is a flex row, `LV_FLEX_ALIGN_END` on the cross axis, so the 48 px temperature and 28 px min/max sit on a common bottom edge without a hand-tuned nudge.

**Type sizes on screen:** clock digits (210), temperature (48), min/max (28). That is the brief's cap of two sizes besides the digits, exactly.
