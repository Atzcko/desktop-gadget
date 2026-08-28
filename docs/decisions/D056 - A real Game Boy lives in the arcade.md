---
title: D056 - A real Game Boy lives in the arcade
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - games
---

# D056 — A real Game Boy lives in the arcade

## Context

Track 2 of the games plan: actual Game Boy software on the clock. The owner
downloaded Peanut-GB themselves (found extracted in ~/Downloads) and said
"go get the gameboy" — covering the one remaining fetch, a libre ROM.

## Decision

- **Peanut-GB, vendored verbatim** into `lib/peanut-gb/` from the owner's
  download (MIT, Mahyar Koshkouei). Sound compiled out (`ENABLE_SOUND 0`) —
  the silence is hardware, not configuration.
- **The screen doubles to 320×288** on a canvas; each emulated line writes
  two canvas rows with doubled pixels in the classic DMG green palette —
  pre-swapped for `LV_COLOR_16_SWAP`, the exact lesson the YouTube
  thumbnails paid for first.
- **The gamepad is touch zones read RAW, two points at once.** D-pad +
  button simultaneously is two fingers, which LVGL's single-point indev can
  never report — `app_touch_points()` extends the trackpad's raw-read
  precedent (D051). Landscape: screen left, pad column right. Portrait:
  screen top, thumbs below, like a phone emulator.
- **Cartridges are files**: `POST /rom` → `/roms/boot.gb`, `tools/rom`
  uploads one, a new cart clears the old save, battery saves persist to
  `/roms/boot.sav` on exit for carts that have RAM. **Homebrew or owned
  ROMs only** — the shipped cartridge is Libbet (pinobatch, zlib, 32 KB),
  fetched from its official release with the owner's go-ahead after Tobu
  Tobu Girl turned out to ship no prebuilt binary.
- Frame pacing: one `gb_run_frame()` per 16 ms on the LVGL task (the
  emulator costs a few ms at 240 MHz), full-canvas invalidate per frame.

## Amended same day (v1.35.1, v1.36.0)

The zones shipped invisible — hitboxes with no chrome, reported by the
owner within the hour. They now draw themselves from the same table the
hit-test reads. And the single `boot.gb` slot became a SHELF: every
`/roms/*.gb` is a cartridge, a picker appears when there is more than one,
and saves are per-cart. `tools/rom` keeps filenames.

## Consequences

- The chooser's third card is live; ~330 KB PSRAM while playing (ROM +
  canvas + core), all freed on exit.
- Peanut-GB is DMG-only: Game Boy Color-exclusive carts refuse politely.
- If a frame ever tears or the pace stutters on hardware, the knobs are
  invalidate-every-second-frame and `PEANUT_GB_HIGH_LCD_ACCURACY` — both
  one-line.

## Related

- [[D055 - Games wear the house style]] · [[D051 - The clock is a trackpad; the mouse is real HID]]
- [[D037 - Apps become Lua scripts]] — uploads-as-content, third instance
