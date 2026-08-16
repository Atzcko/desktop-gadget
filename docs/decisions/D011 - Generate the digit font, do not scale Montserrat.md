---
title: D011 - Generate the digit font, do not scale Montserrat
type: decision
id: D011
date: 2026-08-16
status: accepted
origin: Stage 1
tags:
  - decision
  - typography
---

# D011 — Generate a real 210 px digit font

**Problem.** Fliqlo digits need to be ~150 px tall on a 450 px panel. LVGL 8's bundled Montserrat set **stops at 48 px** (`LV_FONT_MONTSERRAT_48`), and an LVGL 8 `lv_label` cannot be scaled — there is no transform on label text.

**Options considered.**

| Option | Verdict |
|---|---|
| Scale a 48 px label with `lv_img` transform | LVGL 8 labels aren't images; would need render-to-canvas per frame. Blurry and expensive. |
| Draw digits as 10 pre-rendered images | Works, but A8 bitmaps for 10 digits ≈ 470 KB and loses text layout entirely. |
| **Generate a real LVGL font, digits only** | **Chosen.** |

**Implementation.**

```bash
npx lv_font_conv@latest \
  --font "/System/Library/Fonts/Supplemental/Arial Bold.ttf" \
  --size 210 --bpp 4 --format lvgl --lv-include lvgl.h \
  --range 0x30-0x39 --no-kerning --no-compress \
  -o src/fliqlo_digits.c
```

Subsetting to `0x30-0x39` — ten glyphs, no letters, no punctuation — is what keeps a 210 px font affordable. Result: **~90 KB of bitmap data**, 4 bpp antialiasing.

**Resulting metrics**, read back from the generated file:

| | |
|---|---|
| `adv_w` | `1869` (1/16 px) = **116.8 px**, *identical for all ten digits* |
| `box_h` | 148–153 px |
| `line_height` | 154 |
| `base_line` | 3 |

> [!success] Tabular figures, for free
> Every digit has the same advance width. A proportional font would make the clock visibly jitter as digits changed (a `1` narrower than a `0`). Arial's digits are tabular by design, so `HH` occupies exactly 233.6 px regardless of value. **All card geometry derives from this number.**

**Why Arial Bold.** Fliqlo's numerals are a bold neo-grotesque; Arial Bold is the closest match already present on macOS as a plain `.ttf` (Helvetica ships as a `.ttc` collection, which `lv_font_conv` handles less reliably).

> [!note] Font licensing
> Arial is a licensed Monotype face. Embedding a ten-glyph subset in firmware for a personal device is not a distribution concern, but if this project is ever published, swap in a libre grotesque — Inter, Roboto or Archivo — by re-running the one command above. Nothing else in the code refers to the typeface.

**The linkage trap.** `fliqlo_digits.c` compiles as **C**; `ui.cpp` is **C++**. `LV_FONT_DECLARE` expands to `extern const lv_font_t ...`, which under C++ gets a mangled name and fails to link. It must be wrapped:

```cpp
extern "C" {
LV_FONT_DECLARE(fliqlo_digits);
}
```

**Related.** [[Stage 1 - Static digits]]
