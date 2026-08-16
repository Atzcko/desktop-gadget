---
title: D023 - Emotions must change the mode, not decorate it
type: decision
id: D023
date: 2026-08-16
status: accepted
supersedes: the visual design in D018
origin: owner, from hardware use
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

## Per-state motion

Eyes are now the primary carrier, with distinct motion rather than distinct shapes — motion is far more visible at a glance:

| State | Colour | Eyes |
|---|---|---|
| `thinking` | blue | glance side to side, plus a ripple alternating across the cards |
| `working` | blue | slow blink |
| `success` | green | bounce up, both cards ripple once |
| `error` | red | fast horizontal shake |
| `celebrate` | amber | rapid bounce, continuous ripples |
| `sleepy` | amber | flat bars, slow breathing fade |

## The general lesson

Two of this project's bugs have now been *design* failures presenting as *code* failures — this one, and the invisible 5-second hold in [[Settings screen]] before a progress indicator was added. In both cases the mechanism worked perfectly and the human could not tell.

> [!tip] Add the diagnostic before rewriting
> A single `Serial.printf` reporting what the overlay actually created settled "is it broken or is it invisible?" in one flash cycle. Without it, the temptation is to rewrite rendering code that was never at fault.
