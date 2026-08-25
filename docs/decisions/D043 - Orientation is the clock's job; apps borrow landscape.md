---
title: D043 - Orientation is the clock's job; apps borrow landscape
type: decision
status: accepted
date: 2026-08-25
tags:
  - decision
  - ui
  - hardware
---

# D043 — Orientation is the clock's job; apps borrow landscape

## Context

The owner asked for the device to work in any orientation, with BOOT cycling
90° per press. The library supports all four rotations on this panel, touch
remapping included — but every screen in the firmware was laid out for
600×450, and the clock's line-mode zoom renders both cards side by side:
572 px that portrait's 450 cannot hold.

## Decision

Three tiers, by what each screen actually needs:

1. **The clock and the drawer are orientation-native.** Portrait stacks the
   cards — hours over minutes, no colon, which is what Fliqlo itself does on
   a phone — and the drawer's flex row wraps three tiles wide. `ui_init` is
   re-entrant: a live change rebuilds the clock for the new shape, freeing
   the zoom buffer and killing the old build's timers first.
2. **Apps borrow landscape.** Native and script apps alike are 600×450
   layouts; pretending otherwise clips them at x=450. The host rotates the
   panel to the *nearest* landscape (90→0, 270→180) for the duration of an
   app and restores the owner's orientation on the way out. When a
   portrait-capable app exists, the flag goes in the `App` contract then,
   not before.
3. **Portrait line mode is the fade, not the zoom.** The no-canvas fallback
   has existed since the canvas was added; portrait simply never allocates
   the canvas.

**BOOT is the live control** — release under 2 s cycles 0→90→180→270,
persisted. Settings' "Flip 180" switch became a Rotation selector applied on
**Save**: applying live from inside Settings would rotate the very screen
being touched, since Settings itself runs under the borrowed-landscape rule.
`POST /rotate` drives the same path over HTTP, because an orientation feature
nobody can verify without a finger is a checklist item, not a feature
([[D039 - The device must be drivable without a finger]]).

`settings.rotation` (0–3) replaces `rotate_180`, migrating the old NVS bool
so a provisioned device keeps its orientation.

## What the first remote test caught

Asked for 180, landed on 90: a phantom BOOT press. The code trusted an
"external 10 K pull-up" that existed only as an unverified claim in our own
[[T4-S3]] note — the schematic shows none — so plain `INPUT` left the pin
floating. `INPUT_PULLUP` fixed it, the note is corrected rather than quietly
edited, and the lesson joins the battery one from
[[D042 - The battery gauge is a voltage estimate]]: **our own reference notes
are only as good as what was actually checked, and the schematic outranks
them.**

## Consequences

- A rotation rebuilds the clock screen (~instant, PSRAM freed and re-taken);
  apps in progress are closed first — orientation is a device-level act.
- Rotating under a running app is impossible by construction.
- The line display briefly loses its zoom transition in portrait. The fade
  reads fine; the minis land in the same corners.
- One more physical control is spent ([[D027 - The gesture budget]] counted
  gestures; the button budget is now also full — BOOT means orientation).

## Related

- [[D030 - Retire the 3-second hold]] · [[D036 - GPIO0 is readable, never drivable]]
- [[D039 - The device must be drivable without a finger]]
