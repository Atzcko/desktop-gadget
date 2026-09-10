---
title: Desktop gadget — Fliqlo flip clock
type: index
device: LilyGO T4-S3
status: feature-complete-pending-verification
updated: 2026-08-16
tags:
  - index
---

# Desktop gadget — T4-S3 flip clock

Firmware turning a **LilyGO T4-S3** into a minimalist Fliqlo-style flip clock with weather and an HTTP "emotion" API that lets Claude Code animate the device during a working session.

This vault is the project's memory: every decision, why it was made, and what happened at each stage. Vault root = project root, so the firmware and its documentation live together.

## Current state

> [!success] v1.33.0 — running, online, and in daily use
> Abu Dhabi. Clock and weather in four orientations (BOOT cycles 90°), two
> themes, a battery gauge that counts coulombs where it can, eight native
> apps plus Lua scripts, gadget-to-gadget messaging with an unread badge on
> the clock, a BLE trackpad + keyboard for the Mac — and a YouTube app that
> shows the owner's REAL Home (their browser session, read on the Mac) and
> **plays video on the clock** at 320×180 through the companion transcoder,
> with "On Mac" one button away. **Releases ship over Wi-Fi**; crashes are
> read back the same way. Fifty-odd tagged releases, every one confirmed on
> the device via `/health` before being called done.

**Apps** live in `src/apps/` (native) and `/apps/*.lua` in LittleFS
(uploaded live, no reboot — [[D037 - Apps become Lua scripts]]);
[[D026 - Apps are a platform, not a special case]] is why both are one
registry. The remote surface is one page: [[HTTP API]]. The physical side is
[[Enclosure]]; the arc is [[The OS direction]].

| Screen | Reached by |
|---|---|
| Clock *(home)* | resting state |
| Line / activity | `POST /emotion`, BLE NUS, or the MCP tool |
| App drawer | swipe up from the bottom edge |
| Timer · Settings · Lab · Messages · Themes · YouTube · Trackpad · Keyboard · Games · Browser · Equalizer | tap a tile (long-press moves it; order persists) |
| Video player | tapping a video in YouTube ([[D053 - The clock plays video after all, through the Mac]]) |
| Text editor | tapping any field in Settings |
| *back one level* | swipe in from the left edge, or the app's back button ([[D033 - Back goes one level, not home]]) |

## Driving the display

From a shell, anywhere on this machine:

```bash
flipclock say building "ui.cpp"     # prefix the work command
flipclock hush                      # last action of a turn
```

From the Chat app, the `flipclock_say` MCP tool — it exists because the clock is
on a private LAN address that Anthropic's servers cannot reach, so a local
bridge is the only way. See [[CLAUDE]].

## Reference

- [[Module map]] — what each file owns, and what it may not do
- [[T4-S3]] — pins, panel constants, rotation table
- [[LilyGo AMOLED library]] — the API surface we depend on
- [[RELEASES]] — every version, what changed, and why

## Decisions

| # | Decision |
|---|---|
| [[D001 - Use the LilyGO AMOLED library, not TFT_eSPI]] | QSPI panel — TFT_eSPI physically cannot drive it |
| [[D002 - Pin LVGL to 8.4.0]] | Exact pin; LVGL 9 is a port, not an upgrade |
| [[D003 - Copy the library platformio env verbatim]] | Every build flag is load-bearing |
| [[D004 - Rotation 0 is already landscape]] | 600×450 is the default — no `setRotation` needed |
| [[D005 - PlatformIO runs on Python 3.12]] | System Python is 3.14, too new for PIO 6.x |
| [[D006 - Keep build artifacts out of iCloud]] | `.pio` in iCloud is a sync storm + eviction hazard |
| [[D007 - Skip SD, keep charge LED default]] | `beginAMOLED_241(true, false)` |
| [[D008 - Brightness scale]] | 0–255 range; day 90 ≈ 35 %, night 25 |
| [[D009 - LVGL buffer strategy]] | Full-screen 527 KB buffer in PSRAM |
| [[D010 - Night dimming is a schedule, not a sensor]] | T4-S3 has no ambient light sensor |
| [[D011 - Generate the digit font, do not scale Montserrat]] | Montserrat stops at 48 px; LVGL 8 can't scale labels |
| [[D012 - Card geometry]] | Every constant derives from the 116.8 px digit advance |
| [[D013 - Settings live in NVS, config.h is only defaults]] | On-device settings; NVS survives re-flashing |
| [[D014 - Touch hit-testing]] | Every LVGL object is CLICKABLE by default — strip it |
| [[D015 - Abu Dhabi locale]] | `<+04>-4`; brackets required, offset sign inverted |
| [[D016 - Wi-Fi is provisioned on-device]] | The password never leaves the owner's hands |
| [[D017 - BLE and Wi-Fi coexistence]] | One radio: BLE must init first, modem sleep is mandatory |
| [[D018 - Emotion API - one engine, two transports]] | Shared engine, NUS over BLE, same JSON |
| [[D019 - Text entry gets its own screen]] | `LV_EVENT_FOCUSED` is an edge, not a level |
| [[D020 - Humidity recedes by contrast, not size]] | Type-size budget spent; use luminance |
| [[D021 - BLE HID, for discoverability not typing]] | Only a HID profile gets you into macOS Bluetooth settings |
| [[D022 - Custom HID identity, not a keyboard]] | The report descriptor, not the appearance, defines the device |
| [[D023 - Emotions must change the mode, not decorate it]] | Minimal means few elements, not low contrast |
| [[D024 - Emotions as a circumplex, rendered as one line]] | 26 emotions, one renderer: valence→hue, arousal→agitation |
| [[D025 - Scaling text LVGL cannot scale]] | Canvas derives from img, so a picture of text can be zoomed |
| [[D026 - Apps are a platform, not a special case]] | Settings was already an app; name the pattern rather than invent one |
| [[D027 - The gesture budget]] | One finger, four gestures — displacement disqualifies a press |
| [[D028 - Set a number by dragging the number]] | One control per number; the card IS the roller |
| [[D029 - Back is a system gesture, not a widget event]] | Polled above the widget tree, because LVGL 8 does not bubble |
| [[D030 - Retire the 3-second hold]] | Settings is an app; the drawer already goes there |
| [[D031 - The layout is the model]] | Reorder by moving a flex child, never by rebuilding a live screen |
| [[D032 - Three gestures, one control]] | Drag coarse, tap exact, hold repeat — one card does all three |
| [[D033 - Back goes one level, not home]] | app → drawer → clock; one function owns what back means |
| [[D034 - Updates ship over the air]] | POST /update to the ota_1 slot that was always in the table |
| [[D059 - The battery is drawn, not composed]] | iPhone fill with the number cut out: two clipped label draws |
| [[D058 - The equalizer listens through the Mac]] | The Mac hears its own output; the clock gets 32 bar heights, never audio |
| [[D057 - The clock is a browser, through the Mac]] | Headless Chrome renders, the clock shows pixels and sends touch |
| [[D056 - A real Game Boy lives in the arcade]] | Peanut-GB at 2x in DMG greens; two-finger zones; carts are files |
| [[D055 - Games wear the house style]] | Snake and Breakout from the theme table; canvas and object engines proven |
| [[D054 - Home comes from the owner's own session]] | yt-dlp + browser cookies on the Mac; the clock sees ids and titles |
| [[D053 - The clock plays video after all, through the Mac]] | Mac transcodes MJPEG; clock shows honest 320×180 pixels |
| [[D052 - The gadget types, on request]] | Keyboard report joins the map; D022 revised, identity follows function |
| [[D051 - The clock is a trackpad; the mouse is real HID]] | Mouse report in both identities; two fingers read raw |
| [[D050 - YouTube is a dashboard and a remote, not a player]] | Official API + thumbnails on-device; the Mac does the playing |
| [[D049 - The look is a table]] | Themes cascade because every screen is create-on-entry |
| [[D048 - Settings navigates like Apple's]] | macOS sidebar in landscape, iOS stack in portrait, all vertical |
| [[D047 - Messages are conversations, and the clock wears the badge]] | Threads + bubbles; contacts learned from every direction |
| [[D046 - Gadgets message over HTTP and mDNS]] | Identity is the device name; the server was already listening |
| [[D045 - One chrome, every shape]] | One back chip, every app portrait-native; scripts tag in |
| [[D044 - Coulombs where measurable, model where not]] | Charge is counted, discharge is modelled and voltage-tethered |
| [[D043 - Orientation is the clock's job; apps borrow landscape]] | Clock and drawer go portrait; apps get lent a landscape panel |
| [[D042 - The battery gauge is a voltage estimate]] | SY6970 has no fuel gauge; the chip is honest about it |
| [[D041 - Nothing may unwind through a live interpreter]] | A binding may request a lifecycle change, never perform one |
| [[D040 - A script must not be able to reboot the clock]] | -DFOO=0 enables a defined()-guarded macro; and pcall everything |
| [[D039 - The device must be drivable without a finger]] | POST /launch, so UI bugs are reproducible from a shell |
| [[D038 - Crashes must be readable without a cable]] | GET /crash: the core dump partition was always there |
| [[D037 - Apps become Lua scripts]] | Hybrid: C++ core, Lua app layer, no reboot to add an app |
| [[D036 - GPIO0 is readable, never drivable]] | BOOT has a button to GND — readable forever, drivable never |
| [[D035 - The Lab may only touch pins the firmware does not own]] | Whitelist from schematic × board config; on the header ≠ free |

## Releases

- [[RELEASES]] — **v1.17.0**, 2026-08-22. Bump `include/version.h`, log it,
  tag it, `tools/ota`, confirm via `/health`. (Cable only for first install
  and rescue — [[D034 - Updates ship over the air]].)

## Reference

- [[T4-S3]] — the board: pin ownership map, header whitelist, BOOT button
- [[LilyGo AMOLED library]] — the API surface we actually depend on
- [[CLAUDE]] — build/flash commands and working agreements
- [[HTTP API]] — every endpoint and companion tool, one table
- [[The OS direction]] — OTA and scripting done, ELF rejected
- [[Enclosure]] — the Fusion 360 side
- [log.md](log.md) — chronological project lifecycle

## Scope boundaries

> [!warning] Explicitly out of scope for v1
> - ~~BLE HID input — a later phase~~ **Arrived** (v1.29–v1.30): the device
>   is a real BLE mouse and keyboard — [[D051 - The clock is a trackpad; the mouse is real HID]],
>   [[D052 - The gadget types, on request]]. D021's original
>   discoverability-only stance is history, kept for the reasoning.
> - **Battery management / deep sleep** — USB-powered always-on. Battery
>   *telemetry* exists since v1.21.0 (a chip on the clock, `battery` in
>   `/health` — [[D042 - The battery gauge is a voltage estimate]]); reacting
>   to it does not.
> - **SD card** — slot unused, init skipped (its pins are lendable to the
>   [[D035 - The Lab may only touch pins the firmware does not own|Lab]]).
> - **Any cloud beyond Open-Meteo and NTP.** All rendering is local.

## Acceptance criteria for v1 — see [[Acceptance results]]

- [x] Cold boot → correct local time in under 10 s — **4.45 s**
- [ ] Minute rollover, including 23:59 → 00:00 — **needs your eyes**
- [x] `POST /emotion` animates then reverts — **5.2 s**, verified via `/health`
- [x] Wi-Fi drop → clock runs, weather degrades, auto-reconnects — observed in the field
