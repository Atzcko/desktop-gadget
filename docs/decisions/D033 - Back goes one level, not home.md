---
title: D033 - Back goes one level, not home
type: decision
status: accepted
date: 2026-08-21
tags:
  - decision
  - ui
  - apps
---

# D033 — Back goes one level, not home

## Context

The left-edge swipe ([[D029 - Back is a system gesture, not a widget event]])
jumped straight to the clock from anywhere. With one app that was
indistinguishable from "back". With a drawer between the clock and a growing
set of apps it stopped matching the path the user actually walked: leave the
timer to open Settings and the swipe threw you past the drawer to the clock,
and the trip back in was two more gestures.

The owner asked for the stack model, plus visible back buttons in the apps —
the timer had none at all after v1.14.0 removed its Clock button, leaving an
invisible gesture as the only exit.

## Decision

**Back pops one level of the stack the user walked: app → drawer → clock.**
`app_host_back()` owns that meaning, and everything routes through it:

- the left-edge swipe
- the timer's new back card
- Settings' bottom-bar button (was "Close", now "Back")
- the drawer's Clock button

The app-first-refusal hook is unchanged: `running->back()` is still offered the
gesture before the host acts, so Settings with its text editor open closes the
editor and goes nowhere.

**The drawer is rebuilt on every visit, not kept resident.** Backing out of an
app calls the same `build_drawer()` that swipe-up uses. Same discipline as the
apps themselves: nothing stays alive in the background, PSRAM stays flat, and
there is no cached drawer to go stale when the app order changes.

**One door.** The buttons do not call `app_host_home()` or open the drawer
directly; they call `app_host_back()`. When back's meaning changes again there
is exactly one place it changes.

## Consequences

- Leaving an app lands where you launched it from, and one more swipe reaches
  the clock. Two levels, two swipes, no surprises.
- `app_host_home()` still exists — it is what back *does* when only the drawer
  is open, and the escape hatch for anything that genuinely means "the clock,
  now".
- The timer's control row is three cards (back · Start · Reset), which undoes a
  sliver of v1.14.0's "bigger buttons" — Start and Reset went from 268 to
  202 px wide, still 2.6× their original area.
- Supersedes the *destination* half of D029. The architecture half — polled
  above the widget tree, judged on release, app first refusal — stands
  unchanged.

## Related

- [[D029 - Back is a system gesture, not a widget event]]
- [[D031 - The layout is the model]] — load the new screen before deleting the
  old one; `app_host_back()` follows the same rule
- [[D026 - Apps are a platform, not a special case]]
