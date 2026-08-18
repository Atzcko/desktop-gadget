---
title: D032 - Three gestures, one control
type: decision
status: accepted
date: 2026-08-18
tags:
  - decision
  - ui
  - apps
---

# D032 — Three gestures, one control

## Context

The timer's cards became rollers in v1.12.0
([[D028 - Set a number by dragging the number]]) — drag the minutes card for
minutes, the seconds card for seconds. Better than the whole-face drag it
replaced, and still, in the owner's word, **tricky**.

Measured rather than guessed, the reason is arithmetic. At 13 px a step on a
450 px-tall screen, a full-height drag is 34 steps. Setting 45 seconds took two
drags, and landing on an exact number took a third — while a 220 ms fold ran
behind the finger, so the number you were aiming at was never the number on
screen.

Making the drag faster fixes the range and makes the aim worse. They are
opposite problems and one control cannot solve both.

## Decision

**Three gestures on the same card**, each doing the thing it is good at:

| Gesture | Does |
|---|---|
| Drag up / down | continuous, coarse — 18 px a step |
| Tap above / below the middle | exactly ±1 |
| Hold above / below the middle | ±1 repeating, ~10 a second |

The drag can now afford to be *calmer* (13 px → 18 px) because it is no longer
the only way to hit a value, and hold-repeat covers the full range of seconds in
about six seconds.

Three supporting changes:

- **The fold runs at 70 ms a phase, not the clock's 180.** The clock folds once
  a minute and the fold is the point. Here it fires on every step of a drag, and
  an animation slower than the finger is what made the number feel like it was
  lagging behind the intent.
- **Seconds wrap, minutes clamp.** Wrapping puts 59 one tap from 0 in either
  direction. It stays reversible during a drag because the value is recomputed
  from total displacement since touch-down, not accumulated
  ([[D028 - Set a number by dragging the number]]).
- **Above the card's middle is up, below is down.** Nothing to aim at, and it
  reads the same on either card.

## `SHORT_CLICKED`, not `CLICKED`

LVGL sends `LV_EVENT_CLICKED` on every release, including the end of a hold. A
tap handler bound to `CLICKED` would add one extra step to the end of every
hold-repeat. `LV_EVENT_SHORT_CLICKED` fires only when no long press was sent.

The drawer hit the same trap from the other direction
([[D031 - The layout is the model]]).

## Also settled here

- **No frame around a running number.** A coloured border around a number reads
  as an error box. The button says *Pause* and the digits are moving; that is
  the state, shown twice already.
- **The controls are cards.** Same charcoal, same radius, same width, sitting
  directly under the numbers they act on — but **no seam**. That line means
  *this flips*, and drawn through a word it reads as a strikethrough.
- **The Clock button is gone**, so the left-edge swipe
  ([[D029 - Back is a system gesture, not a widget event]]) is the only way
  out of the timer. A gesture with no affordance is a gesture nobody finds, so
  a thin dim bar sits at the left edge saying the edge is live — the same trick
  the clock used for the hold, now retired ([[D030 - Retire the 3-second hold]]).

## Consequences

- Three gestures on one object, which is exactly what D027 says to resist —
  and D027 says why this is allowed: *"an app owns its own screen, so it owns
  its own gestures and has no budget problem."* Only the clock has to stay
  empty.
- `drag_moved` gates the tap and the repeat, so a drag never also counts as a
  tap.

## Related

- [[D028 - Set a number by dragging the number]]
- [[D027 - The gesture budget]]
