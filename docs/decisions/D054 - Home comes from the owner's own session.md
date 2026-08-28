---
title: D054 - Home comes from the owner's own session
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - privacy
---

# D054 — Home comes from the owner's own session

## Context

The owner asked twice for the phone's YouTube: a proper Home. The official
API cannot provide it at any auth level (D050 addendum) — but yt-dlp can
read the owner's own logged-in browser session and fetch
`youtube.com/feed/recommended`: the actual, personalized Home.

## Decision

**The companion serves `/home`** by running yt-dlp with
`--cookies-from-browser` (Chrome, then Firefox, then Safari), returning the
top ~12 recommendations as `{id, title, channel}`. The clock fetches that
over the LAN and builds thumbnails from the public `i.ytimg.com` URLs.

**The privacy line, drawn exactly**: the owner's cookies are read by
yt-dlp, on the owner's Mac, from the owner's browser, and never leave that
machine. The device receives video ids and titles — nothing else. Nothing
is stored anywhere new.

**Home is the app's default view** whenever a companion host is configured
— that is what "like my phone" means. Latest / Popular / Search remain as
chips.

## Consequences

- Home requires: yt-dlp installed, a browser on the Mac logged into
  YouTube, and (first run, Chrome) possibly a keychain approval when yt-dlp
  decrypts cookie storage. Each failure reports as its own message on the
  device.
- The recommendation quality is whatever YouTube gives the browser session;
  the clock adds nothing and cannot filter.
- yt-dlp's cookie extraction is the most breakable link in the whole stack;
  it lives Mac-side where `brew upgrade yt-dlp` fixes it (same argument as
  D053's streaming).

## Also fixed here

Playback failed on most real videos: the `/stream` format selector demanded
MUXED streams and modern YouTube ships many videos adaptive-only.
**Video-only formats are the right ask** — the clock has no audio and
ffmpeg drops it anyway. `bestvideo[height<=360]/…/best`, verified against
the exact id that failed.

## Related

- [[D053 - The clock plays video after all, through the Mac]]
- [[D050 - YouTube is a dashboard and a remote, not a player]]
