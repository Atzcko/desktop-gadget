---
title: HTTP API
type: reference
tags:
  - reference
  - api
updated: 2026-08-28
---

# HTTP API

Every remote capability of the device, one table. Base URL
`http://flipclock.local/` (mDNS) or the IP from `GET /health`. No
authentication — private-LAN posture, accepted in
[[D034 - Updates ship over the air]] and unchanged since.

| Endpoint | Method | Body / notes |
|---|---|---|
| `/health` | GET | Everything: version, git, `current` screen, `rotation`, live `panel`, `theme`, uptime, RSSI, weather, `battery{}` incl. hybrid-gauge `ma`/`mah_used` ([[D044 - Coulombs where measurable, model where not]]), `ble{}` incl. `subs{mouse,key}`, emotion state, PSRAM/heap |
| `/emotion` | POST | `{"state","duration_s","message"}` — the line display ([[D018 - Emotion API - one engine, two transports]]) |
| `/update` | POST | raw `firmware.bin` → inactive OTA slot; use `tools/ota` ([[D034 - Updates ship over the air]]) |
| `/crash` | GET / DELETE | last panic: task, PC, backtrace, `same_build`; decode with `tools/crash` ([[D038 - Crashes must be readable without a cable]]) |
| `/launch` | POST | `{"name":"Timer"\|"clock"\|"drawer"}` — open any screen ([[D039 - The device must be drivable without a finger]]) |
| `/rotate` | POST | `{"deg":0\|90\|180\|270}` — same path as the BOOT button ([[D043 - Orientation is the clock's job; apps borrow landscape]]) |
| `/theme` | POST | `{"n":0..}` — apply a theme, rebuild in place ([[D049 - The look is a table]]) |
| `/apps` | GET / POST / DELETE | Lua apps: list / upload (`octet-stream`, `?name=`) / remove ([[D037 - Apps become Lua scripts]]) |
| `/msg` | POST | `{"from","text"}` — deliver a message TO this device ([[D046 - Gadgets message over HTTP and mDNS]]) |
| `/messages` | GET | conversation history + `unread` ([[D047 - Messages are conversations, and the clock wears the badge]]) |
| `/send` | POST | `{"ip","text"}` — make THIS device send |
| `/rom` | POST | raw `.gb` body → the Game Boy's cartridge slot ([[D056 - A real Game Boy lives in the arcade]]) |
| `/youtube` | GET / POST | status (incl. `streaming`, `frames`) / `{"key"}` `{"channels"}` `{"host"}` `{"region"}` `{"mode":0-3}` `{"search":"q"}` `{"play_here":"id"}` `{"stop":true}` ([[D050 - YouTube is a dashboard and a remote, not a player]], [[D053 - The clock plays video after all, through the Mac]], [[D054 - Home comes from the owner's own session]]) |

Bad input returns 4xx with a reason; nothing here reboots the device except
a successful `/update`.

## Companions on the Mac

| Tool | Role |
|---|---|
| `flipclock` (global CLI) | drive the line display from any shell |
| `tools/ota` | build + push + verify a release |
| `tools/crash` | fetch + addr2line the last panic |
| `tools/app` | upload / list / remove Lua apps |
| `tools/ytserve` | :8999 companion — `/play` opens in the browser, `/stream/<id>` transcodes to 320×180 MJPEG (yt-dlp + ffmpeg, D053), `/home` serves the owner's real recommendations from their browser session (D054) |
