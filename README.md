# Desktop gadget — a LilyGO T4-S3 flip clock

Firmware for a 2.41″ AMOLED desk clock that turned into a small operating
system. It shows the time as a Fliqlo-style split-flap, and it also plays
Game Boy games, browses the web, types on my Mac over Bluetooth, and draws a
spectrum analyser of whatever the Mac is playing.

Everything ships over Wi-Fi. The USB cable is for first install and rescue.

```
Timer · Settings · Lab · Messages · Themes · YouTube · Games · Browser
Equalizer · Trackpad · Keyboard      (+ any Lua script you upload)
```

## What it does

- **A flip clock first.** Split-flap digits with a two-phase fold, weather,
  four orientations (the BOOT button cycles them), two themes, and an
  anti-burn-in walk. The clock is the resting state of the object; everything
  else is a layer above it.
- **Apps are a platform, not special cases.** Eleven native C++ apps
  implement one `App` contract — create on entry, destroy on exit — and the
  drawer picks them up. A twelfth kind exists: **Lua scripts**, uploaded
  live over HTTP and compiled on the device, no reboot.
- **Updates over the air.** `tools/ota` builds, pushes to `POST /update`, and
  only reports success once `/health` answers with the new version. Crashes
  are read back the same way: `tools/crash` decodes the stored core dump to
  file and line without a cable.
- **Real BLE HID.** The clock is a genuine Bluetooth trackpad and keyboard —
  no companion involved, the Mac just sees an input device.
- **A Mac companion for what the hardware cannot do.** A small Python server
  (`tools/ytserve`) lends the clock a browser engine, a video decoder and an
  ear. Details below.

## The companion pattern

Some things an ESP32-S3 genuinely cannot do. Rather than fake them, the Mac
does the impossible half and the clock renders honest pixels:

| Feature | What the Mac does | What the clock does |
|---|---|---|
| **Browser** | Headless Chrome renders the page | Decodes JPEG frames, sends taps/scrolls/keys back |
| **YouTube** | `yt-dlp` + `ffmpeg` transcode to MJPEG | Plays it at 320×180, or throws it to the Mac |
| **Equalizer** | ScreenCaptureKit + FFT on the system audio | Draws 32 bands, 30 fps |

The privacy line is the same in all three: **nothing leaves the Mac but
pixels and numbers.** The browser sends coordinates and receives JPEGs, never
a cookie. The equalizer sends thirty-two loudness values — bar heights, not
audio.

## This repo is also a vault

The directory is simultaneously a PlatformIO project and an
[Obsidian](https://obsidian.md) vault. Start at [`index.md`](index.md).

- **[`docs/decisions/`](docs/decisions)** — 59 decision records. Every
  structural choice, with the reasoning and the trade-off that made it. They
  are written so the same ground is not re-litigated later, and several are
  superseded in place rather than deleted.
- **[`docs/RELEASES.md`](docs/RELEASES.md)** — 68 releases, each with what
  changed and why.
- **[`log.md`](log.md)** — the chronological record, including the bugs. The
  silent failures are in here on purpose: a released `SCStream` that stops
  without an error, a keyboard descriptor rejected for having two
  collections, thumbnails with crossed bytes on a `LV_COLOR_16_SWAP` panel.
- **[`graphify-out/graph.html`](graphify-out)** — the whole project as a
  navigable knowledge graph: 2,779 nodes, 9,791 edges, 92 communities,
  linking each decision to the code that implements it.

## Hardware

**LilyGO T4-S3** — ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM, 2.41″ RM690B0
AMOLED (450×600) over QSPI, CST226SE capacitive touch, SY6970 PMU.

Notes and pin ownership: [`docs/reference/T4-S3.md`](docs/reference/T4-S3.md).
The enclosure (Fusion 360, printable STLs) is in [`3D print/`](3D%20print).

## Build

PlatformIO, with LVGL pinned to 8.4.0 (9.x does not link against the vendor's
helper — see D002).

```bash
cp include/config.example.h include/config.h    # defaults only; no secrets
export PATH="$HOME/.platformio-venv/bin:$PATH"
pio run
```

First flash needs the cable:

```bash
pio run -t upload --upload-port /dev/cu.usbmodemXXXX
```

After that, never again:

```bash
tools/ota
```

**Wi-Fi credentials are never in a file.** They are typed on the device and
stored in NVS, which survives re-flashing (D016). The same goes for the
YouTube API key.

## HTTP API

The device exposes a small API on `flipclock.local` — health, emotion
display, OTA, crash readback, app launching, rotation, messaging, and one
endpoint per companion feature. Full table:
[`docs/reference/HTTP API.md`](docs/reference/HTTP%20API.md).

```bash
curl http://flipclock.local/health
curl -X POST http://flipclock.local/emotion \
  -H "Content-Type: application/json" \
  -d '{"state":"celebrate","duration_s":5}'
```

There is no authentication. It is a private-LAN device and that posture is a
deliberate, documented choice (D034) — do not expose it to the internet.

## Third-party code

Vendored with their licenses intact:

- **Lua 5.4.7** — MIT, © 1994–2024 Lua.org, PUC-Rio (`lib/lua/`)
- **Peanut-GB** — MIT, © 2018–2023 Mahyar Koshkouei (`lib/peanut-gb/`)
- **Libbet and the Magic Floor** — zlib, © Damian Yerrick (`roms/`, see
  [`roms/README.md`](roms/README.md))
- **LilyGo AMOLED Series** library and **LVGL 8.4.0**, pulled by PlatformIO

The firmware itself carries no license file yet, which means default
copyright — all rights reserved. If you want to reuse it, open an issue.
