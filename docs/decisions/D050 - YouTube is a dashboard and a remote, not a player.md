---
title: D050 - YouTube is a dashboard and a remote, not a player
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - api
---

# D050 — YouTube is a dashboard and a remote, not a player

> [!note] Title half-superseded
> Since [[D053 - The clock plays video after all, through the Mac]] it IS a
> player (companion-transcoded MJPEG), and since
> [[D054 - Home comes from the owner's own session]] it has the real Home.
> The assessment below was the honest map at the time and the seams it
> predicted are the ones those features slotted into.

## Context

The owner asked whether a YouTube app is possible. The honest assessment:
watching on-device is walled off three ways — no hardware codec (software
H.264 is single-digit fps on this CPU), stream acquisition is the signed-URL
cat-and-mouse that breaks monthly, and the board has no audio path at all.
The owner chose Tier A: the official Data API, thumbnails, and tap-to-play
throwing the video to the Mac.

## Decision

- **Official API only** (Data API v3, API key). Key-only auth cannot read
  the owner's subscriptions, so the app shows the latest uploads of channels
  the owner CONFIGURES (`@handle` list, up to six). Cost per refresh: one
  `channels.list` per un-cached handle, then one `playlistItems.list` per
  channel — ~12 units of the daily 10 000. No `search.list` (100 units each)
  in the daily path.
- **The key is provisioned like Wi-Fi credentials** (D016): typed on-device
  or POSTed once over the LAN, kept in the module's NVS, never in a file,
  never in the repo.
- **Thumbnails decode on the worker task** through the ESP32-S3 ROM's
  TJpgDec: `mqdefault.jpg` is 320×180, the decoder's ½ scale lands exactly
  on the 160×90 row size, and the LVGL task only ever blits finished RGB565
  buffers from PSRAM (D018 kept).
- **Playing = the companion.** `tools/ytserve` on the Mac listens on :8999;
  a tapped row POSTs the video id; the Mac opens the watch page. The device
  stores the companion IP. When the companion is down, the app says so and
  nothing else breaks.
- **TLS without certificate pinning, stated plainly**: public read-only
  data on a desk gadget; the CA-bundle maintenance does not pay for the
  wrong-thumbnail failure it would prevent.

## Consequences

- Quota math means the app could refresh every few minutes all day and not
  dent the free tier; it refreshes on open and on demand instead.
- Titles are ASCII-rendered by the montserrat fonts — emoji and CJK in
  video titles show as blanks. Known, cosmetic, not worth a font today.
- ~230 KB PSRAM for eight thumbnails, freed on every refresh.
- A future OAuth device-flow could add real subscriptions; the seam is
  `do_refresh()`, nothing above it.

## Addendum, 2026-08-28 — Popular and Search (v1.31.0)

The owner asked for "home and suggested videos". **The official API cannot
provide either**: the personal Home feed and recommendations are not exposed
at all (the related-videos endpoint was removed in 2023), and subscriptions
need OAuth. What shipped instead, honestly labelled:

- **Popular** — `chart=mostPopular` for a region (default AE), one quota
  unit. Trending is the closest Home surrogate a key can buy.
- **Search** — real `search.list`, 100 units a query (≈100/day within the
  free tier), typed on the on-screen keyboard, on demand only.
- Latest (channels) stays the default. Mode chips at the top switch;
  `POST /youtube {"mode":n}` / `{"search":"q"}` / `{"region":"US"}` drive it
  remotely.

Also fixed here: thumbnails rendered with crossed bytes — `LV_COLOR_16_SWAP`
is 1 for this panel, so TRUE_COLOR buffers must be stored pre-swapped.

## Related

- [[D046 - Gadgets message over HTTP and mDNS]] — the worker-task pattern
- [[D016 - Wi-Fi is provisioned on-device]] · [[D039 - The device must be drivable without a finger]]
