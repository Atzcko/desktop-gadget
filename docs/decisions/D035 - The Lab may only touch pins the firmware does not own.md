---
title: D035 - The Lab may only touch pins the firmware does not own
type: decision
status: accepted
date: 2026-08-22
tags:
  - decision
  - apps
  - hardware
---

# D035 — The Lab may only touch pins the firmware does not own

## Context

The owner asked for the device to double as a bench tool: set GPIOs, scan
I2C, talk serial. The dangerous part is not the features — it is that on this
board most pins are spoken for, and one careless write is not an error
message, it is a corrupted panel or a crashed chip:

| Write to | Result |
|---|---|
| 10–16, 18 | display QSPI / TE — panel corruption |
| 6 / 7 | PMU + touch I2C — hangs the bus that runs charging |
| 9 | PMIC enable — can kill power |
| 5, 8, 17 | PMU / touch IRQ + reset |
| 33–37 | OPI PSRAM — instant crash |
| 19 / 20 | USB — lose the console |
| 0, 45, 46 | strapping |

## Decision

**A whitelist is the design.** The app was built by cross-referencing the
schematic (`T4-S3-240719.pdf`, header P5) against the library board config —
what physically reaches the header, minus what the firmware owns:

> **21, 38, 39, 40, 41, 42, 47, 48** — free
> **43, 44** — UART0, labelled TX0/RX0, the UART tab's default
> **1, 2, 3, 4** — shared with the SD socket, labelled, fine with no card

**GPIO18 is the cautionary tale**: it is ON the header, and it is the
display's tearing-effect signal. On the header ≠ free. It is excluded.

Three per-domain rules:

- **The internal I2C bus (6/7) is scannable, never drivable.** An address
  probe is a read; the scanner may run on the live PMU/touch bus (it shows
  whatever is on the little P4 plug too). The external scanner uses the
  second controller, `Wire1`, on whitelist pins only — reconfiguring `Wire`
  would yank the bus out from under the touch driver.
- **UART is `Serial1`** — `Serial` is the USB console. Any whitelist pin
  pair via the GPIO matrix; 43/44 default because that is what they are.
- **Leave no trace**: `destroy()` returns every whitelist pin to Hi-Z,
  closes `Serial1`, ends `Wire1`. Launching and leaving the Lab is
  electrically invisible.

## Known sharp edges, accepted

- The three tabs share the whitelist and trust the user: opening the UART on
  a pin the GPIO tab is driving is allowed, last-configured wins. It is a
  bench tool for its owner, not a product.
- I2C scan runs synchronously in the LVGL task (~150 ms freeze). Cosmetic.
- No SPI tab yet; pins 1–4 are the natural home when wanted.

## Related

- [[D026 - Apps are a platform, not a special case]]
- [[D034 - Updates ship over the air]] — first feature shipped entirely OTA
