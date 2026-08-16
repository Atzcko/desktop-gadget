---
title: Releases
type: index
tags:
  - release
---

# Releases

Semantic versioning; the scheme and the release procedure live in [[CLAUDE]].
The version is reported by the boot log, `GET /health` and Settings ▸ Info,
each with a compiler build stamp so a stale flash is detectable.

## v1.1.0 — 2026-08-16

**Every value now wears the same card.** The resting weather block changed from
plain text labels to three charcoal cards with centre seams at 44 px, matching
the clock's design language. Hierarchy is carried by scale — the clock is 4.8×
the type size — and by colour, rather than by two different treatments on one
screen.

- New face `fliqlo_mid` (44 px, `0-9 ° / %`) for the weather cards, alongside
  `fliqlo_small` (38 px) for the line-mode strip. Two scales so the strip stays
  visibly subordinate.
- `mini_card()` is now scale-parameterised and shared by both rows.
- Weather sits at y 330, centred in the space the clock cards leave.
- Stale marker is a 10 px dot beside the cards — still a shape, never a type
  size.

Flash usage **fell** 92 KB despite two new faces: dropping the last reference to
`lv_font_montserrat_48` let the linker discard it.

RAM 18.1 % (59 468 B) · Flash 26.3 % (1 723 357 B).

## v1.0.0 — 2026-08-16

First tagged release. Feature-complete against the original brief, plus
everything added during the build.

**Clock.** Fliqlo split-flap on true black, 600×450 landscape, a purpose-
generated 210 px digit font with tabular advance, two-phase fold animation on
minute change, NTP with a POSIX timezone, daily resync.

**Weather.** Open-Meteo on a dedicated FreeRTOS task — current temperature,
today's min/max and humidity, 15-minute refresh, capped exponential backoff,
stale marker, and never on the render path.

**Line display.** 32 states as points in Russell's circumplex: arousal drives
amplitude, frequency and speed; character drives shape. One continuous white
polyline built from three incommensurate harmonics with a speech envelope and
an end taper. Reachable over HTTP, BLE NUS and a BLE HID output report.

**Settings.** On-device, opened by a 3-second hold: Wi-Fi scan and join,
timezone, city search, brightness and night hours, BLE identity, diagnostics.
Persisted to NVS and survives re-flashing.

**Bluetooth.** Custom vendor HID identity so macOS lists and pairs the device
without mistaking it for a keyboard, with a keyboard descriptor kept as a
compatibility fallback.

**Care.** Brightness schedule, ±2 px burn-in walk, tap and long-press gestures.

**Measured:** cold boot to correct local time **4.45 s** against a 10 s budget;
emotion auto-revert at 5.2 s; malformed API input returns 4xx with no reboot;
Wi-Fi loss degrades gracefully and reconnects with backoff.

RAM 18.1 % (59 444 B) · Flash 27.7 % (1 815 465 B).
