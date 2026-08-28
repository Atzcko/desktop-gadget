---
title: D053 - The clock plays video after all, through the Mac
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - api
---

# D053 — The clock plays video after all, through the Mac

## Context

The owner asked for the mobile-app experience: play videos ON the device,
with sending to the Mac as an option. This is Tier B of the original
assessment — D050 titled the app "not a player" because the walls were the
codec, stream acquisition, and audio. Two of those walls are the Mac's to
carry; one is physics.

## Decision

**The Mac transcodes; the clock decodes honest pixels.** `tools/ytserve`
gains `/stream/<id>`: yt-dlp resolves the direct URL (the cat-and-mouse
lives on the Mac where updating it is `brew upgrade`, never in firmware),
ffmpeg emits baseline MJPEG at **320×180, 12 fps, ~9 q** — a bitrate the
Wi-Fi budget yawns at. The clock's worker reads the stream into a rolling
buffer, frames on SOI/EOI, decodes the LATEST complete frame and drops
stale ones — natural frame-skip when decode (~35 ms) lags. The LVGL task
only ever blits the front of a double buffer.

**Tap a video → it plays on the clock.** The player shows native-size
pixels (no upscale blur), the title, and two exits: the standard back chip,
and **"On Mac"** — which closes the local stream and sends the id to the
companion's `/play`, the old behavior demoted to the option it should be.

**Silent, and said so.** This board has no DAC, no amp, no speaker (T4-S3
note). The player is a moving picture. The day a MAX98357A lands on the Lab
pins, audio becomes an I²S task; until then, no pretending.

## Consequences

- D050's title is half-superseded: dashboard, remote — and now a player,
  with the Mac as its codec. The seam held exactly where D050 predicted.
- Playback occupies the yt worker: refresh and Mac-sends queue behind a
  stop. Leaving the app or the player always stops the stream.
- yt-dlp is a Mac-side install the owner makes (`brew install yt-dlp`);
  until then the player reports "companion: install yt-dlp" and everything
  else works.
- ~230 KB more PSRAM while playing (two 320×180 buffers), freed never —
  allocated once on first play, reused.

## Related

- [[D050 - YouTube is a dashboard and a remote, not a player]] — the
  assessment this upgrades · [[D038 - Crashes must be readable without a cable]]
