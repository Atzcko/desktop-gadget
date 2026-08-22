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

> [!success] v1.17.0 — running, online, and in daily use
> Abu Dhabi. Clock, weather, the 32-state line display over HTTP / BLE / MCP,
> an app platform with three apps (Timer, Settings, Lab), stack navigation,
> and **OTA — releases ship over Wi-Fi**; the cable is rescue-only. Eighteen
> tagged releases; every one confirmed on the device via `/health` before
> being called done.

**Apps** live in `src/apps/`; adding one is a file and a line —
[[D026 - Apps are a platform, not a special case]]. The bigger arc is
[[The OS direction]]: OTA done, scripted apps next. The physical side is
[[Enclosure]].

| Screen | Reached by |
|---|---|
| Clock *(home)* | resting state |
| Line / activity | `POST /emotion`, BLE NUS, or the MCP tool |
| App drawer | swipe up from the bottom edge |
| Timer · Settings · Lab | tap a tile (long-press moves it; order persists) |
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
- [[The OS direction]] — OTA done, scripting next, ELF rejected
- [[Enclosure]] — the Fusion 360 side
- [log.md](log.md) — chronological project lifecycle

## Scope boundaries

> [!warning] Explicitly out of scope for v1
> - **BLE HID input** (actually sending keystrokes/media keys) — still a later phase. The device now *presents* a HID keyboard service so macOS will pair with it, but sends no reports: [[D021 - BLE HID, for discoverability not typing]].
> - **Battery / deep sleep** — USB-powered always-on.
> - **SD card** — slot unused, init skipped (its pins are lendable to the
>   [[D035 - The Lab may only touch pins the firmware does not own|Lab]]).
> - **Any cloud beyond Open-Meteo and NTP.** All rendering is local.

## Acceptance criteria for v1 — see [[Acceptance results]]

- [x] Cold boot → correct local time in under 10 s — **4.45 s**
- [ ] Minute rollover, including 23:59 → 00:00 — **needs your eyes**
- [x] `POST /emotion` animates then reverts — **5.2 s**, verified via `/health`
- [x] Wi-Fi drop → clock runs, weather degrades, auto-reconnects — observed in the field
