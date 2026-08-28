---
title: D052 - The gadget types, on request
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - ble
---

# D052 — The gadget types, on request

## Context

The owner asked for a keyboard app after the trackpad. Typing needs a HID
keyboard usage — the exact thing
[[D022 - Custom HID identity, not a keyboard]] was built to avoid
presenting, back when the fear was macOS *mistaking* the gadget for a
keyboard it was not.

## Decision

**A standard keyboard collection (Report ID 3) joins both report maps**,
beside the mouse (ID 2). D022 is revised, not repealed: the vendor identity
still leads, the device still calls itself a desktop gadget — but it now
genuinely IS a text-input device when the owner opens the Keyboard app, and
an identity that follows function is not a mistake.

> [!warning] Keyboard Setup Assistant may appear ONCE after re-pairing
> That dialog exists to identify unknown keyboards' layouts. Close it or
> click through — HID usage codes are layout-independent and the clock
> types US-mapped codes regardless.

- The UI is the stock LVGL keyboard with **no textarea**: keys go to the
  HOST, not to a field. `kb_cb` translates button text through an
  ASCII→usage table (the same table every firmware keyboard ships) and
  arrows/backspace/enter map to their usage codes; mode keys stay local.
- `ble_key()` sends press **and release as one call** — a stuck modifier on
  the host must not depend on a second call happening.
- A dim 24-char tail shows what was typed, as feedback only. Nothing is
  stored; the tail is on the screen, so the same discretion applies as to
  any keyboard visible in a room.

## Consequences

- One more re-pair (descriptor changed again — third identity revision).
  After this the HID surface is complete: vendor + mouse + keyboard.
- Six-key rollover and held modifiers beyond shift are not implemented;
  this is a message-typing keyboard, not a gaming one.
- Non-US host layouts will render some symbols differently — usage codes
  name positions, not glyphs. Known, accepted.

## Related

- [[D022 - Custom HID identity, not a keyboard]] — revised by this
- [[D051 - The clock is a trackpad; the mouse is real HID]]
