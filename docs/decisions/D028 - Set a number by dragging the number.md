---
title: D028 - Set a number by dragging the number
type: decision
status: accepted
date: 2026-08-17
tags:
  - decision
  - ui
  - apps
---

# D028 — Set a number by dragging the number

## Context

The timer shipped in v1.11.0 with a single drag target: the whole face. Drag
anywhere, minutes change. Seconds were unreachable.

The owner asked for "the layout like in the *Night from* section in settings,
but for every number", while keeping the current design, and with the digits
animating like a flip clock. Those three read as contradictory on first pass —
*Night from* is an `lv_roller`, and a roller looks nothing like a flip card.

They are not contradictory. *Night from* is not a roller **because rollers are
good**; it is a roller because it gives **one control per number**. That is the
part being asked for. The design that stays is the flip card. So:

> **each card becomes its own roller.**

## Decision

Every settable number is its own drag target, and the thing you drag is the
thing that displays it. Minutes card sets minutes; seconds card sets seconds.

Three properties fall out, and all three were bugs waiting to happen otherwise:

**The value is absolute, not accumulated.** Each `PRESSING` event recomputes
from total displacement since `PRESSED` (`(drag_y0 - y) / PX_PER_STEP`) rather
than adding a delta. Accumulating drifts, and worse, it makes the gesture
irreversible — drag down to undo and you land somewhere else. Absolute means
returning your finger to where it started returns the number to where it
started, exactly.

**A fold in progress is not queued behind.** Dragging generates changes far
faster than a 220 ms fold completes. A card already folding takes the new value
immediately and animates the *next* transition; it does not stack. Stacked
folds render as tearing.

**The card keeps `LV_OBJ_FLAG_CLICKABLE`.** It is the control, so it is the one
object on the screen that must NOT be passed to `decor()`.
See [[D014 - Touch hit-testing]].

## The fold is duplicated, deliberately

`flip_to()` here is a second copy of the clock's split flap, not a shared
helper. Extracting the original would mean refactoring the one screen that has
worked all day in order to add a feature somewhere else. A short duplicate is
the cheaper risk today. If a third caller appears, extract it then — two is not
yet a pattern.

## Consequences

- Both numbers are reachable, and which card you are changing is unambiguous
  because you are touching it.
- The timer stops looking like a different app that happens to show numbers.
  It is the same object as the clock, doing something else.
- One more place that knows the fold. Noted above, and accepted with a trigger.

## Related

- [[D026 - Apps are a platform, not a special case]]
- [[D027 - The gesture budget]]
- [[D014 - Touch hit-testing]]
