---
title: D020 - Humidity recedes by contrast, not size
type: decision
id: D020
date: 2026-08-16
status: accepted
origin: owner
tags:
  - decision
  - layout
---

# D020 — Humidity recedes by contrast, not by size

**Ask.** Show relative humidity, "discrete" — present but not competing for attention.

**The constraint that decides it.** The brief caps the design at **two type sizes besides the clock digits**. Those are already spent: 48 px for the current temperature, 28 px for min/max. A third, smaller size for humidity would break the rule and, worse, start the slide toward a dashboard.

**Decision.** Humidity uses the **existing 28 px size** and recedes through **contrast** instead:

| Element | Size | Colour |
|---|---|---|
| Current temp | 48 | `#FFFFFF` |
| Min / max | 28 | `#8A8A8A` |
| **Humidity** | **28** | **`#4E4E4E`** |

Roughly half the luminance of the min/max grey. On a true-black AMOLED that reads as present-but-secondary — you see it when you look for it and your eye skips it when you do not. A 14 px left pad separates it from the min/max pair so it reads as its own fact rather than a third temperature.

> [!tip] Contrast is the free axis
> Once a layout has spent its type-size budget, luminance is the remaining way to express hierarchy. It is also the *right* axis on an AMOLED, where a dim grey costs almost no light and never competes with the white digits.

**Missing data.** Open-Meteo may omit `relative_humidity_2m`. The parser defaults it to `-1`, and the UI renders an empty string rather than a confident `0%`. A wrong reading is worse than an absent one.

**Toggle.** `Settings ▸ Screen ▸ Humidity`, alongside the existing Weather and Burn-in guard switches. Persisted as the NVS key `hum`.

**Also exposed** in `GET /health` as `humidity_pct` (and `temp_c`), so the Python client and diagnostics see the same numbers the panel does.
