---
title: D024 - Emotions as a circumplex, rendered as one line
type: decision
id: D024
date: 2026-08-16
status: accepted
supersedes: D023
origin: owner + literature research
tags:
  - decision
  - design
  - emotion
---

# D024 — Emotions as a circumplex, rendered as one line

**Ask.** Move the clock and weather to a corner during an emotion, animate the emotion in and out, research how many emotions exist, and render them on a **single line** — 80s, minimal, and *not* the dashed thing v3 produced.

## The research, and what it changed

| Model | Count | Note |
|---|---|---|
| Ekman (1992) | **6** | anger, disgust, fear, happiness, sadness, surprise |
| Plutchik | **8** | primaries in 4 opposing pairs, with intensity layers |
| Cowen & Keltner (2017) | **27** | categories **"bridged by continuous gradients"** |
| Russell (1980) circumplex | **∞** | any emotion = a point on 2 axes |

The two findings that mattered are the last two. Cowen & Keltner's headline number is 27, but their actual claim is that the categories are **not discrete** — they shade into one another. Russell's circumplex says the same thing constructively: every emotion is a point in a 2D space of **valence** (unpleasant ↔ pleasant) and **arousal** (calm ↔ activated).

> [!success] The circumplex is why a single line works
> Two axes, and a line has exactly two obvious free parameters:
>
> - **arousal → how agitated the line is** — amplitude, frequency, speed
> - **valence → its hue** — red/magenta at −1, violet-blue at 0, cyan-green at +1
>
> That is not a metaphor stretched to fit; the mapping is direct. High arousal genuinely looks agitated, and the valence axis lands naturally on a synthwave palette.

**So the renderer implements the space, not a list.** There is no per-emotion animation code. 26 named emotions share one renderer, and adding another is **one row in a table** — never new drawing code. Blends would be meaningful too, because the space is continuous.

### Coverage

26 named points spanning all four quadrants: Plutchik's eight primaries, Ekman's six, and the working states this device actually needs (`thinking`, `working`, `searching`, `focused`, `waiting`, `success`, `error`…).

A `character` field layers shape on top of the two axes, because valence and arousal alone cannot distinguish *angry* from *afraid* — both are high-arousal and negative:

| Character | Shape | Used by |
|---|---|---|
| `SMOOTH` | clean sine | most states |
| `JAGGED` | triangle wave — hard and angular | anger, error, frustrated, surprise |
| `TREMOR` | fast judder on a slow carrier | fear, confused, disgust |
| `SCAN` | a swell travelling along the line | searching, waiting, anticipation |
| `DROOP` | asymmetric, sags below the axis | sad, disappointed, relief |

## Why v3 looked dashed

v3 drew 30 rectangles with gaps between them. That is a dashed line, and the owner called it exactly that.

v4 draws a **polyline**: 49 sampled points, 48 `lv_draw_line` segments with `round_start`/`round_end` set, so the caps overlap at every joint and the stroke is continuous. Two passes — a wide dim one (width 17, 40 % opacity) under a narrow bright one (width 6) — fake the neon bloom the panel cannot produce optically.

## v5 — white, thin, and talking rather than oscillating

Owner: *"make the line white and thin"* and *"now it's just a sinusoid, I was thinking more of a dynamically moving line mimicking talking, thinking."*

Both notes are right, and the second is the substantive one. **A single sine is periodic and symmetric, so it reads as a graph.** Speech does not look like that. Three ingredients change the reading:

| Ingredient | Why |
|---|---|
| **Additive harmonics at incommensurate ratios** — 1 : 2.27 : 4.13 | irrational ratios mean the sum never repeats on screen, so motion looks organic instead of looped |
| **A speech envelope over time** — two slow oscillators at unrelated rates, multiplied | produces bursts and pauses the way talking does, instead of a constant-amplitude drone |
| **A taper across x** — `sin(pi*u)^0.75`, exactly zero at both ends | the single biggest cue: a stroke running edge to edge is a chart; one that swells in the middle and dies at the ends is a voice |

Amplitude modulation is scaled by arousal, so calm states barely modulate and activated ones burst hard.

> [!note] Valence no longer shows as hue
> The stroke is now pure white — a 2 px core over a 9 px dim pass, which is not decoration: it stops a thin white line from looking like a rendering artefact on a black AMOLED. Valence therefore expresses itself entirely through **motion character** — `DROOP` sags for sadness, `JAGGED` clips hard for anger — rather than colour. If colour is ever wanted back, the HSV mapping is one line.

## The clock steps aside

An emotion is now a **mode change**, animated:

| Element | Clock mode | Emotion mode |
|---|---|---|
| Big cards | centre | fade out, slide up 34 px |
| Small clock | hidden | fades in **top-left**, 48 px |
| Weather | centre, y 302 | slides to **top-right**, y 14 |
| Line | — | fades in at centre, full width |

The 210 px digit font cannot be scaled — LVGL 8 has no usable transform for text — so the corner clock is a **separate 48 px label** kept in step, not a shrunken version of the same object.

Entrance and exit are driven by a gain ramp **inside the wave timer**, not by `lv_anim`. That is deliberate: a rapid emotion change would otherwise leave two animations fighting over one shared gain value.

## Sources

- [Cowen & Keltner — emotion taxonomy discussion](https://emotionresearchgroup.wordpress.com/wp-content/uploads/2019/06/emotion-review_akron.pdf)
- [GoEmotions: A Dataset of Fine-Grained Emotions](https://arxiv.org/pdf/2005.00547)
- [The Circumplex Model of Affects](https://www.morphcast.com/blog/circumplex-model-of-affects/)
- [Circumplex Model of Affect: Valence and Arousal Explained](https://psychologyfanatic.com/circumplex-model-of-arousal-and-valence/)
