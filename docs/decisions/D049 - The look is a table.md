---
title: D049 - The look is a table
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - ui
  - apps
---

# D049 — The look is a table

## Context

The owner wants selectable themes: the current Fliqlo look, and a second
inspired by two references — a board of vivid bento widgets (blue / orange /
red / yellow rounded cards on black) and a round device drawing generative
pixel patterns. "Cascade to other things as well."

## Decision

**A theme is a ~12-field color table** (`theme.h`), read at BUILD time by
everything that draws cards: the clock (hour and minute cards may differ —
Pop makes them blue and red), the colon dots (Pop: yellow), the weather pair
(orange / blue), the drawer tiles (palette cycled by index), the timer's
cards and the standard chip. The background is black in every theme: the
panel is an AMOLED, black is free, and the burn-in budget was calibrated to
it.

**Applying a theme is a rebuild, not a repaint.** Every screen on this
device is create-on-entry (D026), and the rotation machinery (D043/v1.24.1)
already rebuilds the clock and re-opens the current app in place. So the
Themes app saves the choice and calls exactly that — the new look is
standing everywhere before the finger lifts. `POST /theme {"n":1}` does the
same through a rebuild request that skips the rotation no-op guard.

**The Themes app previews with the theme's own colors** — swatch strips are
built from the same table that will paint the real screens, so the preview
cannot drift from the truth.

Not attempted, recorded: the second reference's generative pixel art is a
SCREEN (a live pattern), not a palette. If it comes, it comes as a theme
field driving the emotion line's renderer or an idle screen — a different
feature than colors.

## Consequences

- Fliqlo stays byte-identical as theme 0. Anything unthemed (Settings, Lab,
  Messages chrome — deliberately neutral working screens) reads the same in
  both.
- The zoom canvas renders both mini cards in the hour-card color; in Pop the
  live minute card is red while its zoomed ghost is blue for the 350 ms of
  the transition. Seen, judged cosmetic, accepted.
- A third theme is one table row plus nothing.

## Related

- [[D045 - One chrome, every shape]] · [[D043 - Orientation is the clock's job; apps borrow landscape]]
- [[D026 - Apps are a platform, not a special case]]
