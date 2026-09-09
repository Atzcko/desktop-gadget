---
title: D058 - The equalizer listens through the Mac
type: decision
status: accepted
date: 2026-09-09
tags:
  - decision
  - apps
  - api
  - privacy
---

# D058 — The equalizer listens through the Mac

## Context

The owner wants an equalizer that reacts to anything the Mac plays — music,
videos, anything. The clock has no microphone, and a microphone would be the
wrong instrument anyway: it hears the room, not the mix. The signal that
matters is the Mac's own output, and only the Mac can hear that.

## Decision

**The Mac captures its own output; the clock draws the bars.** The companion
pattern, fourth time (D053, D054, D057), with the lightest payload yet.

- **Capture**: `tools/eq_capture` (Swift) uses ScreenCaptureKit with
  `capturesAudio` — the path OBS takes on modern macOS. No virtual-audio
  driver, no rerouting the owner's sound, nothing installed. Chrome, Music,
  a video in Safari: all of it, because it is the system mix.
- **DSP, in Accelerate**: Hann window, 2048-point real FFT at 48 kHz, 32
  log-spaced bands from 40 Hz to 16 kHz, peak-in-band, dB. A slow AGC makes
  the running peak "full scale" so a quiet podcast fills the bars as well
  as a loud track. Attack 0.60 / decay 0.14 per frame. Output: one line of
  64 hex characters per frame at 30 fps — **2 KB/s**.
- **Relay**: `GET /eq/stream` on the companion. One helper runs while any
  client listens and is released four seconds after the last one leaves —
  nothing captures while nobody looks. ytserve builds the helper itself
  with `swiftc` when the binary is missing or stale.
- **On the clock**: `eq.cpp` reads lines into 32 levels; the app smooths
  again on the LVGL tick (fast rise, slow fall) and drops a peak cap on
  each bar that falls under gravity — the classic analyzer. Every color is
  the theme's (D049): bars fade from the digit color into the card color,
  caps are the colon. Geometry from the live display, so both shapes (D045).

**The privacy line, the tightest yet.** Audio never leaves the Mac. The
clock receives thirty-two loudness numbers thirty times a second — not
audio, not words, nothing reconstructable. Bar heights.

**The permission, stated on the tin.** ScreenCaptureKit requires the
one-time macOS grant "Screen & System Audio Recording", given to whichever
app launched the helper (Terminal, or Claude when driven from there). The
helper reports `!grant …` and that status travels the whole path to the
panel: the clock tells the owner what to click, and granting it is
self-healing — the clock reconnects every two seconds until levels arrive.

**The demo seam.** `?demo=1`, or `EQ_FORCE_DEMO=1` on ytserve, streams a
synthetic spectrum with no helper and no permission. It is how the app was
verified on the panel before the permission existed — the owner's
"I see bars moving" — and it stays, as D039's each-half-on-its-own rule.

## Consequences

- Eleventh native app. The companion now wants the Xcode Command Line Tools
  (for `swiftc`); without them the clock reads "swiftc not found".
- A status from the companion must outlive the clock's reconnect loop, or
  the panel flickers between it and "listening for sound…" every two
  seconds (v1.39.0 shipped with that; fixed in the same release).
- Two companion lessons, recorded because they cost real time: a `pkill`'d
  ytserve takes over a second to die (Chrome threads), so a one-second
  sleep let the old one keep the port and the new one fail silently; and
  under `nohup` Python's stdout is block-buffered, so its log was empty
  while it ran perfectly — `python3 -u`, always.

## The bug that shipped first

v1.39.0's first hour "didn't work" for real audio while the demo bars
danced — the helper logged *capturing system audio* and then emitted
nothing. The `SCStream` was a local inside the startup `Task`; when the
task finished, the stream was **released, and a released SCStream stops
silently**. Retaining it (`gStream`) fixed everything at once. The helper
now also reports its audio format and a level line every ten seconds, so
the next silence is diagnosable from the companion log alone. Verified:
Float32 non-interleaved at 48 kHz, speech peaking ~0.12, bands reaching 255.

## Related

- [[D053 - The clock plays video after all, through the Mac]] ·
  [[D057 - The clock is a browser, through the Mac]] — the companion pattern
- [[D049 - The look is a table]] — why the bars reskin with the theme
- [[D039 - The device must be drivable without a finger]] — the demo seam
