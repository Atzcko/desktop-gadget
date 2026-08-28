---
title: D055 - Games wear the house style
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
---

# D055 — Games wear the house style

## Context

The owner asked for 8-bit games, both recreated classics and — separately
assessed — a real Game Boy. One Games app holds all of it: a chooser, then
the game, with the in-app back stack the platform already taught every
other app.

## Decision

**Native games first, in the device's own language.** Snake and Breakout
are not skinned emulations; they are drawn from the theme table — the snake
is the digit color, the food is the chip color, Breakout's brick rows cycle
the tile palette, the ball is the colon yellow in Pop. Scores render in
`fliqlo_mid`. Switching themes reskins the arcade for free (D049's cascade
doing its job).

**Touch grammar per game, no buttons on the field**: Snake steers by swipe
(reversal into yourself is ignored); Breakout's paddle rides the finger and
a tap serves. Death → score + best (NVS) + tap to retry. Both games lay out
from the live display: the snake grid and brick wall are computed, not
constant, so portrait simply gets a taller field (D045).

**Game Boy is the third card, honestly greyed**: it needs Peanut-GB's
source and a homebrew ROM — downloads the owner approves, not bundled
silently. The card says so.

Engine notes for the next game: Snake proves the canvas path (grid redrawn
per step, PSRAM buffer, allocated on enter and freed on leave); Breakout
proves the object path (float physics with ms-delta, "english" off the
paddle edge, axis-picked brick bounce). Between them, most 8-bit genres
have a template.

## Consequences

- Ninth native app; the drawer scrolls, as designed.
- `esp_random()` seeds gameplay — no Date/rand() ceremony.
- Sound: none, and games designed FOR silence rather than missing it.

## Related

- [[D049 - The look is a table]] · [[D045 - One chrome, every shape]]
- [[D053 - The clock plays video after all, through the Mac]] — the
  companion pattern Game Boy's ROM upload will reuse
