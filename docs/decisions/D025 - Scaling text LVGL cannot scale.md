---
title: D025 - Scaling text LVGL cannot scale
type: decision
id: D025
date: 2026-08-16
status: accepted
origin: owner
tags:
  - decision
  - lvgl
  - animation
---

# D025 — Scaling text that LVGL cannot scale

**Ask.** "When I say animate I meant animate — so it *scales* the clock and puts it in the corner, and the weather just *moves* to the other corner."

What was built was a **cross-fade**: the big clock faded out while a separate small clock faded in. Different thing entirely, and the owner was right to call it out.

## The obstacle

**LVGL 8 cannot transform text.** `transform_zoom` applies to images; a label is drawn glyph-by-glyph from a compiled bitmap face at one fixed size. There is no continuous scale of a 210 px font.

## Routes considered

| Route | Verdict |
|---|---|
| Swap fonts mid-flight (210 → 50) | a visible pop; the pop *is* the thing being animated |
| Animate card size and let digits clip | digits crop rather than shrink — reads as damage |
| `lv_snapshot_take()` → zoom the image | **blocked**, see below |
| **Render into `lv_canvas` → zoom that** | **chosen** |

### Why not snapshot

`lv_snapshot` is exactly the right API and it ships with LVGL, but the library's `lv_conf.h` sets `LV_USE_SNAPSHOT 0`. Adding a project `include/lv_conf.h` does **not** fix it: inspecting the actual compile command shows LVGL's own sources are given only two include paths —

```
-I.pio/libdeps/t4s3/lvgl
-I.pio/libdeps/t4s3/lvgl/src
```

— so `lv_snapshot.c` can never see a project config. A `#error` probe confirmed the project copy reaches *project* translation units only. `LV_CONF_PATH` would work, but it is stringified by macro and this project's path contains a space (`Desktop gadget`). The project `lv_conf.h` was deleted rather than left in the tree looking authoritative while having no effect on the library.

> [!warning] Verify which config a library actually sees
> A config file that is present, correct, and never read is worse than none — it invites changes that silently do nothing. The `#error` probe answers it in one build.

## The chosen route

`lv_canvas` **derives from `lv_img`** (`.base_class = &lv_img_class`), so `lv_img_set_zoom()` works on it. The big clock is drawn into a 572×232 canvas — two rounded rects, the digits at 210 px, the seams — and *that image* is zoomed and flown to the corner. LVGL cannot transform text, but it can transform a picture of text.

Buffer: `572 × 232 × 2 = 265 KB`, `ps_malloc` into PSRAM. Measured on device as a 268 KB drop in free PSRAM.

## Making the handover invisible

The zoom lands on **real cards**, not on a permanently downscaled bitmap — a 4× downsample of a 210 px face would look soft, and the clock must stay crisp for as long as it is on screen.

For the swap to be imperceptible, the landing geometry has to equal the flight geometry exactly. Everything derives from one number, **zoom 61/256 = 0.2381**, chosen so 210 px digits land at exactly 50 px — which is why `fliqlo_corner` is 50 px:

| | big | × 0.2381 | corner card |
|---|---|---|---|
| card | 268 × 232 | 63.8 × 55.2 | **64 × 55** |
| radius | 26 | 6.2 | **6** |
| gap | 36 | 8.6 | **9** |
| digit padding | 17.2 | **4.1** | (64 − 2×27.8)/2 = **4.1** ✓ |

The padding matching to a tenth of a pixel is the check that matters: it means the glyphs are in the same place before and after the swap.

## Weather

"Just moves", as asked — no scale. The row keeps its 44 px cards and slides to the opposite corner. The shift is **measured, not assumed**, because the row's width changes with its values:

```cpp
right = lv_obj_get_x(w_hum.root) + lv_obj_get_width(w_hum.root);
shift = scr_w - 14 - right;      /* right-align against the far edge */
```

## Fallback

If `ps_malloc` cannot provide the canvas, `layout_emotion()` falls back to the old cross-fade rather than failing. A transition that is less pretty beats a clock that does not transition.
