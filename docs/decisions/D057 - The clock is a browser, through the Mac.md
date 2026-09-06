---
title: D057 - The clock is a browser, through the Mac
type: decision
status: accepted
date: 2026-09-06
tags:
  - decision
  - apps
  - api
  - privacy
---

# D057 — The clock is a browser, through the Mac

## Context

The owner asked for a web browser app. A from-scratch browser for the modern
web is impossible on this board and saying otherwise would be a lie: sites
ship megabytes of JavaScript that assume a JIT engine, gigabytes of RAM and a
GPU; this board has 8 MB of PSRAM and none of the rest. It cannot hold a
site's script bundle, let alone run it.

But "the device renders the HTML" was never the only browser architecture.
Opera Mini, Puffin and Amazon Silk were real, shipped browsers that rendered
on a server and sent the client a picture. That is exactly the pattern the
YouTube player already proved here (D053): the Mac does what the device
can't, the device shows honest pixels.

## Decision

**The Mac renders; the clock is a thin client.** The companion drives
headless Chrome (Playwright, `channel="chrome"` — the SYSTEM Chrome 151, so
no separate Chromium is ever downloaded) and streams JPEG frames — the exact
wire format the YouTube player decodes. The clock decodes the latest frame
into a full-screen RGB565 double buffer and blits it; taps, scrolls and keys
go back up as POSTed input events that Chrome replays.

- **Pixels down**: `GET /browse/stream?w=&h=` — concatenated JPEGs, framed on
  SOI/EOI, latest-wins. The device sets the viewport to its exact page-view
  size, so coordinates map 1:1.
- **Input up**: `POST /browse/input` — `tap` (click), `scroll` (wheel),
  `key` (type text or press a named key), `nav`, `back`/`forward`/`reload`.
  Two device worker tasks: one reads the stream, one drains an input queue
  and POSTs; neither touches LVGL.
- **The app**: toolbar (back / forward / reload / URL / keyboard), a page
  view, and a touch layer that turns a drag into a wheel scroll and a tap
  into a click. URL editing and page typing each get a keyboard overlay.
  A phone-shaped viewport gets a phone layout (mobile user-agent).

**Privacy line, same as D054.** Chrome browses with the Mac's own logged-in
sessions; nothing leaves the machine but pixels. The device sends coordinates
and keystrokes, receives JPEGs — never a cookie, never a credential.

**The honest costs**, stated on the tin: it only works while the companion
runs, and it is a remote-control feel with real latency (~5 fps on device),
not a snappy local browser. The rendering fidelity is Chrome's, complete.

## Consequences

- Ninth… tenth native app. `browser.cpp` reuses yt.cpp's TJpgDec path
  verbatim in spirit (RGB888 → 565, pre-swapped for `LV_COLOR_16_SWAP`).
- ~1 MB PSRAM while open (two full-screen buffers + a 256 KB stream buffer),
  freed on exit; the worker joins before the buffers are released so nothing
  decodes into freed memory.
- The companion grew past its name: `tools/ytserve` now also serves
  `/browse*`, backed by `tools/browser_session.py`. It needs
  `pip install playwright` once (already installed); without it the browser
  endpoints 503 with that message and YouTube keeps working.
- The companion host is shared: `browser_begin()` seeds the IP from
  `yt_play_host()`, so a device already set up for YouTube needs no new setup.

## The tiers not taken

- **Native reader** (on-device HTML parse, no companion): real work for a
  result that fails on essentially every mainstream site. Rejected as the
  primary; a possible future add for offline reading.
- **Companion reader-mode** (Mac de-clutters to article text): fast and
  crisp but article-shaped only — no logins, no interaction. A cheap future
  add on top of this same companion.

## Related

- [[D053 - The clock plays video after all, through the Mac]] — the companion
  streaming pattern this reuses
- [[D054 - Home comes from the owner's own session]] — the same privacy line
- [[D051 - The clock is a trackpad; the mouse is real HID]] — raw touch → input
