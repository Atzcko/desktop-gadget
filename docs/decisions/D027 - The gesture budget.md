---
title: D027 - The gesture budget
type: decision
id: D027
date: 2026-08-17
status: accepted
origin: the "do not break the others" constraint
tags:
  - decision
  - touch
---

# D027 — The gesture budget

The owner's constraint on the app platform was *"make sure you don't break
others."* On a screen with exactly one input — a finger — every new gesture is
carved out of the same budget, so it is worth writing down what that budget is
before spending more of it.

## Currently spent

| Gesture | Window | Action |
|---|---|---|
| Tap | < 400 ms | date + weather sync age |
| Long press | 1.2 – 3.0 s | cycle brightness |
| Hold | ≥ 3.0 s | Settings |

All three are **time-classified on release** by one handler on the layout root.
Nothing is classified by position.

## Adding the fourth: swipe up from the bottom

That is the hazard. **A swipe begins as a press**, so a naive addition means an
upward drag also registers as a tap, or — worse — as a long press that silently
changes the brightness on the way to opening the drawer.

Three rules keep them disjoint:

1. **Displacement disqualifies a press.** Record the touch-down point. If the
   finger has moved more than ~30 px on release, it was never a tap or a long
   press, whatever the duration was.
2. **The swipe must start at the bottom edge** (`y > height − 80`). A drag
   across the middle of the clock is not a drawer request, and confining the
   origin keeps the rest of the screen free for future gestures.
3. **Suppress the hold indicator once movement starts.** The progress bar
   appearing during a swipe would advertise a hold the user is not performing.

> [!warning] Every gesture makes the next one harder
> Four is close to the limit for a screen with no affordances. A fifth should be
> resisted unless it can be shown to be disjoint from all four — and the honest
> alternative is usually a button inside an app, where there is room for a label
> saying what it does.

> [!note] One of the four has since been retired
> The 3-second hold to open Settings is gone —
> [[D030 - Retire the 3-second hold]]. The reasoning below stands; the hold is
> simply the gesture whose reason expired first, once Settings became an app.
> A system back gesture was added in its place, and it lives only inside apps —
> [[D029 - Back is a system gesture, not a widget event]].

## Inside apps

An app owns its own screen, so it owns its own gestures and has **no budget
problem** — it can use real buttons with real labels. Only the home screen is
constrained, because only the home screen has to stay empty.
