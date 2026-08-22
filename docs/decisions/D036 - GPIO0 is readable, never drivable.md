---
title: D036 - GPIO0 is readable, never drivable
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - hardware
  - apps
---

# D036 — GPIO0 is readable, never drivable

## Context

The Lab shipped in v1.17.0 with a 14-pin whitelist that omitted GPIO0
([[D035 - The Lab may only touch pins the firmware does not own]]). The owner
noticed, and reasonably: they had asked days earlier whether the BOOT button
could be used as a normal button, and the answer was **yes** — a strapping pin
is only special at reset. Then the bench tool that exists to poke pins did not
list it. Two true statements that add up to a confusing product.

## Decision

**GPIO0 appears in the Lab, with Hi-Z / In PU / In PD and no Out.**

The asymmetry is a hardware fact, not caution:

> [!warning] A pin with a button hard-wired to ground must never be an output
> The BOOT button connects GPIO0 to GND permanently. Set the pin to Out and
> drive it high, and one press shorts the pad to ground through nothing but
> the button — current limited only by trace resistance against a pad rated
> ~40 mA. No other whitelist pin has a permanent low-side short attached to
> it, which is why no other pin needs this rule.

Two supporting reasons, neither sufficient alone:

- **It is the rescue path.** There is no OTA rollback
  ([[D034 - Updates ship over the air]]), so BOOT-at-reset is the only way
  back from an image that crash-loops. Anything that risks the pad risks the
  recovery.
- **It is still a strapping pin.** Held low as reset is released, the ROM
  enters USB download mode. Firmware driving it is one unlucky watchdog reset
  from a device that looks bricked.

Reading it costs nothing and delivers exactly what was promised: the BOOT
button as an extra input, visible live in the Lab. The external 10 K pull-up
means Hi-Z already reads **H** at rest and **L** while pressed, with no mode
change needed.

## The index trap this created

The I²C and UART tabs select pins by dropdown position, indexed straight into
`PINS[]`. Adding an entry at the front would have shifted every selection by
one — a scan running on the wrong pins, with no error anywhere.

Input-only pins are therefore **absent from the bus dropdowns**, via a
`bus_map[]` built once at app entry rather than by an ordering convention on
the array. An ordering convention would have worked today and broken silently
the first time someone inserted a pin in the middle.

`apply_mode()` also refuses `PM_OUT` on an input-only pin even though the UI
cannot request it. The UI is one edit away from being wrong; the pad short is
permanent.

## Consequences

- The Lab shows 15 pins; GPIO0 is first, where someone looking for it looks.
- Bus dropdown defaults are unchanged — `bus_map` skips exactly the one added
  pin, so IO47/48 and IO43/44 keep their positions.
- Anyone wanting BOOT to *do* something in normal firmware still can; this
  decision governs the Lab's pin poking, not the button's use as an input
  elsewhere.

## Related

- [[D035 - The Lab may only touch pins the firmware does not own]] — the
  whitelist this amends
- [[T4-S3#The BOOT button (GPIO0)]] — the pin's behaviour
- [[Enclosure]] — the case must keep BOOT pressable for the same reason
