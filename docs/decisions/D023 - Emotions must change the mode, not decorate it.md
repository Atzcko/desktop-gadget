---
title: D023 - Emotions must change the mode, not decorate it
type: decision
id: D023
date: 2026-08-16
status: accepted
supersedes: the visual design in D018
origin: owner, from hardware use
revisions: v1 seam line -> v2 eyes -> v3 neon wave
tags:
  - decision
  - design
  - emotion
---

# D023 — Emotions must change the mode, not decorate it

**Report.** "When you do that it keeps displaying time and I can't see emotions."

## The design was wrong, not the code

The serial diagnostic added while investigating confirms the overlay was always being built:

```
[ui] emotion 5 rendered: eyes=yes seam=on digits=dimmed msg=yes
```

So this was never a rendering bug. It was a **design failure**, and an obvious one in hindsight. For four of the six states — `thinking`, `working`, `success`, `error` — the entire visual signal was a **3 px accent line** on the card seam. That was competing with 150 px white digits on a panel running at brightness 251. Of course it was invisible.

> [!warning] "Minimalist" is not the same as "faint"
> The brief asked for minimalist animations consistent with the aesthetic, and I read that as *quiet*. But an expression nobody notices has failed completely — a subtle signal is worth nothing if the thing it competes with is a hundred times larger and brighter. Minimal means **few elements**, not **low contrast**.

## Decision

The fix is not a brighter accent. It is to make the device visibly change **mode**:

| Change | Effect |
|---|---|
| Clock digits → **30 % opacity** | the clock recedes; still legible, no longer dominant |
| Cards gain a **4 px border** in the state colour | the frame itself signals the state |
| Seam accent **3 px → 10 px**, pulsing | readable across a room |
| **Eyes for every state**, not just two | reads instantly as "it is expressing something" |
| Caption **20 px → 28 px**, in the state colour | actually legible |

The clock is still there and still correct — it simply stops being where your eye lands. Everything reverts precisely on `duration_s`, and the resting screen is untouched: the accent bar is fully transparent and the border is zero-width at rest, so the default Fliqlo face is exactly as the brief specifies.

## v2 — eyes. Also wrong.

Adding eyes made the state visible but produced something genuinely ugly: on hardware they read as **pale blobs colliding with the numerals**, not as a face. The owner's photo settled it in one look, and the redirection was better than the fix I would have reached for:

> "When I say emotions it doesn't have to be with eyes. It can be something like the 80s vibe line that moves and changes color."

That is a far better idea than a face. A line does not compete with the digits for the same screen real estate the way an eye-shaped blob does — it occupies a band, and the clock reads straight through it.

## v3 — the neon wave

A synthwave sine line running **through the card seam**, drawn every frame at ~30 fps:

- 30 segments, each with a dim oversized pass underneath to fake a neon bloom (the panel has no real glow; on true black this reads convincingly)
- hue computed per segment in **HSV** and rotated over time, so the line genuinely shifts colour rather than switching between fixed ones
- the seam is already this design's line, so the wave reads as *that line coming alive* rather than a new element bolted on
- the digits stay at 30 % opacity and remain perfectly legible

| State | Motion | Hue |
|---|---|---|
| `thinking` | slow, gentle amplitude | cyan → blue, slow drift |
| `working` | quick, tight, low amplitude | blue, faster drift |
| `success` | broad confident swell | green |
| `error` | hard alternating spikes (squared-off wave) | red, no drift |
| `celebrate` | fast and tall | **full 360° spectrum**, fast rotation |
| `sleepy` | barely moving, shallow | violet, almost static |

## The performance decision that made it possible

Drawn as **one object with a custom `LV_EVENT_DRAW_MAIN` callback**, not 30 moving objects.

> [!warning] Thirty moving objects would have redrawn the whole screen
> Each moved object queues invalidated rectangles — up to 60 per frame. LVGL's invalid-area buffer is finite (`LV_INV_BUF_SIZE`); overflow it and LVGL abandons partial redraw and repaints **everything**: 600×450×2 = 540 KB per frame, 16 MB/s at 30 fps, against a QSPI ceiling of roughly 18 MB/s. It would visibly stutter.
>
> One custom-drawn object yields exactly one invalid region — the 600×96 band, ~115 KB per frame, ~3.4 MB/s. Comfortable, and the reason this can run at 30 fps on top of everything else.

## The general lesson

Two of this project's bugs have now been *design* failures presenting as *code* failures — this one, and the invisible 5-second hold in [[Settings screen]] before a progress indicator was added. In both cases the mechanism worked perfectly and the human could not tell.

> [!tip] Add the diagnostic before rewriting
> A single `Serial.printf` reporting what the overlay actually created settled "is it broken or is it invisible?" in one flash cycle. Without it, the temptation is to rewrite rendering code that was never at fault.
