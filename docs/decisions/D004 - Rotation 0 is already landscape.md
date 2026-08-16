---
title: D004 - Rotation 0 is already landscape
type: decision
id: D004
date: 2026-08-16
status: accepted
origin: discovered in source
tags:
  - decision
  - display
---

# D004 — Rotation 0 is already 600×450 landscape

**Finding.** I expected to need a `setRotation()` call to get from a portrait 450×600 panel to the landscape 600×450 the brief asks for. That is wrong.

`initSequence.h` defines the panel as:

```c
#define RM690B0_WIDTH   600
#define RM690B0_HEIGHT  450
```

and `setRotation(0)` for `BOARD_AMOLED_241` sets `_width = boards->display.width` (600), `_height = boards->display.height` (450). `beginAMOLED_241()` already ends with `setRotation(0)`.

**Decision.** Do **not** call `setRotation()`. Take the default.

> [!note] Superseded in part, v1.9.0
> The owner wanted the panel upside down, so `setRotation()` *is* called now —
> with **rotation 2**, the other landscape orientation. Same 600×450, and the
> library re-applies the CST226SE swap/mirror alongside it, so touch follows
> the screen without any work here. Exposed as `Settings ▸ Screen ▸ Flip 180`.
>
> The finding below still holds and is the reason this was one line: landscape
> is native, so flipping is a choice between two landscape rotations rather
> than a re-layout.

**Why not call it anyway "for clarity".** Calling `setRotation(0)` a second time is harmless but it re-issues `LCD_CMD_MADCTL` and re-applies the touch transform after the touch driver is already configured. Fewer moving parts is worth more than a redundant line of self-documentation — the fact is recorded here and in [[T4-S3]] instead.

**Consequence.** `amoled.width()` → 600, `amoled.height()` → 450 immediately after `begin`. All layout constants derive from those calls, never from literals, so a future orientation change stays a one-line edit.

> [!note] The 16 px offset
> Landscape rotations carry `_offset_y = 16` internally. This is applied inside `pushColors` by the driver. Application and LVGL coordinates remain 0-based over 600×450.
