---
title: D044 - Coulombs where measurable, model where not
type: decision
status: accepted
date: 2026-08-26
tags:
  - decision
  - hardware
  - battery
---

# D044 — Coulombs where measurable, model where not

## Context

The owner asked for a real coulomb counter — count the mA consumed, base the
percentage on that — instead of the voltage estimate from
[[D042 - The battery gauge is a voltage estimate]].

The register map settles what is possible. The SY6970 measures **charge
current** (REG 0x12, 50 mA steps) but has **no discharge-current ADC and no
accumulator**: nothing in the silicon can see current leaving the battery. A
true coulomb counter requires hardware in the battery path.

## Decision

**A hybrid gauge (`gauge.cpp`), honest about which half is which:**

| Phase | Method |
|---|---|
| Charging | integrate **measured** charge current — genuinely coulombs |
| Charge complete | snap to 100 % — the one absolute anchor this hardware offers |
| Discharging | integrate a **modelled** draw (88 mA base + 82 mA × brightness/255), tethered to the voltage curve with a ~50 min time constant |

The tether is what makes the model safe: its error cannot accumulate past
what the voltage curve allows, while day-to-day the reading is smooth and
monotonic where raw voltage sagged under load and jumped on unplug.

State persists in NVS (min 1 write/min, normally on ≥1 % change) and survives
reboot; a stored value more than 25 points from the voltage curve at boot is
discarded — the battery changed while we were off. `settings.batt_mah`
(default 5000, the owner's pack) scales the arithmetic. `/health` reports
`gauge:"hybrid"`, signed `ma`, and `mah_used` since last full charge.

## Two more library traps, recorded

- `isChargeDone()` returns the **opposite** of its name (`!=` where `==`
  belongs). `chargeStatus()` is read directly.
- `isCharging()` counts DONE as charging. Charging is now
  `PRE_CHARGE || FAST_CHARGE`, which also fixes the chip showing green on a
  full battery.

## The real thing, when wanted

A true counter is a ~3 € part on the battery lead: an INA226 (shunt) or a
fuel-gauge IC (LC709203F, MAX17048) between pack and board, on the free I²C
pins the Lab already exposes (47/48 or the P4 plug). The gauge module is the
seam: `gauge_update()` is the only consumer of current data, so detected
hardware can replace the model without touching anything above it.

## Related

- [[D042 - The battery gauge is a voltage estimate]] — superseded in part:
  voltage is now the tether, not the gauge
- [[D035 - The Lab may only touch pins the firmware does not own]] — the bus
  a real gauge would ride on
