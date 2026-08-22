---
title: Lab
type: stage
status: awaiting-hardware-check
date: 2026-08-22
tags:
  - stage
---

# Stage — the Lab

The device as a bench tool: GPIO control, I²C scanner, UART monitor.
Decision and the whitelist that is the actual design:
[[D035 - The Lab may only touch pins the firmware does not own]].
Pin ground truth: [[T4-S3#Pin map — who owns what]].

## Scope

1. GPIO tab — 14 whitelist pins, four modes, live levels, tap-to-toggle.
2. I²C tab — `Wire1` scanner on any whitelist pair (default 47/48),
   100/400 kHz, plus read-only scan of the internal 6/7 bus.
3. UART tab — `Serial1` on any pair (default 43/44), 9600–230400, 512-byte
   RX tail, canned sends.
4. Leave no trace: exit returns every pin to Hi-Z, ends `Serial1` and `Wire1`.

## Acceptance — needs the owner's bench

- [ ] An output toggled **H** drives the header pin (LED or meter confirms)
- [ ] Internal I²C scan lists the PMU and touch controller
- [ ] TX jumpered to RX echoes the canned sends in the monitor
- [ ] Re-entering the Lab shows every pin back at Hi-Z
- [ ] External scan with nothing attached reports "no devices" (no false ACKs)
