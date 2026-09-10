---
title: D059 - The battery is drawn, not composed
type: decision
status: accepted
date: 2026-09-10
tags:
  - decision
  - ui
---

# D059 — The battery is drawn, not composed

## Context

The owner asked for the iPhone battery: a solid fill that empties as the
charge drops, with the percentage inside it — inverted where it sits over
the fill, normal where the fill has drained away. The old chip was an
outline with a label child. An LVGL label is one color; it cannot be black
on its left half and white on its right.

## Decision

**The chip draws itself.** `bat_body` keeps its border and gets a
`LV_EVENT_DRAW_MAIN` callback that paints, in order: the fill (inner area,
width = charge), then the number **twice** — once in the background color
with the draw context's clip area narrowed to the fill, once in the chip
color clipped to everything right of the fill's edge. A digit straddling
the edge changes color mid-stroke, exactly as on the phone. The label
object is gone; `ui_set_battery` stores the percentage and color and
invalidates the body.

The fill color is the chip color: the theme's digit white, green while
charging, red under 15 % — the same three meanings as before, now carried
by the fill as well as the outline.

## Consequences

- Two `lv_draw_label` calls per redraw of a 46×22 object — free.
- Clip-narrowing is the reusable trick: anything that needs "this text,
  two colors, split at a line" is this callback with a different edge.
- The battery, the unread badge and the line display still share the
  corner rules of D047; nothing about visibility changed.

## Related

- [[D044 - Coulombs where measurable, model where not]] — where the percentage comes from
- [[D049 - The look is a table]] — why the fill is the digit color
