---
title: D045 - One chrome, every shape
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - ui
---

# D045 — One chrome, every shape

## Context

Two owner complaints, one root: *rotation needs to be reflected in the menu
and every app with no text overlap*, and *the back button should be uniform
and findable*. D043's borrowed-landscape was honest scaffolding — apps turned
sideways in portrait — and every app had grown its own back button in its own
corner: the timer's in a control row, Settings' at mid-bar right, the Lab's
bottom-left, the drawer's bottom-centre saying "Clock".

## Decision

**One chip.** `app_host_std_back(parent, cb)`: bottom-left, 132×56, charcoal,
"← Back", on every app and the drawer. A custom `cb` only where leaving needs
a guard (Settings' unsaved-changes prompt). Layouts reserve the bottom-left
156×76. The drawer's says Back, not Clock — the label names the gesture, not
the destination.

**Every native app lays itself out for both shapes**, reading
`lv_disp_get_hor_res/ver_res` at create:

| App | Portrait strategy |
|---|---|
| Timer | cards stack (minutes over seconds), like the portrait clock |
| Settings | tabview and keyboard go PCT; the bar became the uniform strip |
| Lab | absolute grids became wrapping flex rows; tab area is computed |
| Messages | born both-shaped: list just gets taller |

**The contract records it**: `App.portrait_ok`. True suppresses D043's
borrowed landscape; false keeps it — which is now only the scripts' default.
A script opts in with a `portrait_ok` tag in its FIRST line (the host decides
rotation before the script runs, so the flag cannot live inside the program)
and reads `SCREEN_W`/`SCREEN_H` globals to actually branch.

## Consequences

- Rotating then opening any native app stays in your orientation. Scripts
  still borrow landscape until tagged.
- The timer's Start/Reset shrank from 122-tall cards to 56-tall strip keys —
  uniformity beat v1.14.0's "bigger buttons" where the two collided; they
  remain 200 px wide.
- Absolute-grid layouts in the Lab became flex; a future pin added to a row
  wraps instead of clipping.

## Related

- [[D043 - Orientation is the clock's job; apps borrow landscape]] —
  superseded for native apps, still true for untagged scripts
- [[D033 - Back goes one level, not home]] — what the chip does
