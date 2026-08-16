---
title: D007 - Skip SD, keep charge LED default
type: decision
id: D007
date: 2026-08-16
status: accepted
origin: brief
tags:
  - decision
  - init
---

# D007 — `beginAMOLED_241(true, false)`

**Decision.** Initialise the board with the T4-S3-specific entry point and skip the SD card:

```cpp
amoled.beginAMOLED_241(/*disable_sd=*/true, /*disable_state_led=*/false);
```

**Why explicit rather than `begin()`.** `begin()` auto-detects the variant by probing I²C. That is the right call for a library demo that must run on four different boards; for firmware that only ever runs on one, it is unnecessary probing at every boot and a silent-wrong-branch risk. We know the hardware.

**Why `disable_sd = true`.** The brief says the SD slot is unused. Passing `true` skips `SPI.begin()` and `SD.begin()`, which:

- frees GPIO 1, 2, 3, 4 and an SPI peripheral,
- removes an `SD.begin()` timeout from the boot path, which matters against the **<10 s cold-boot-to-time** acceptance criterion.

**Why `disable_state_led = false`.** The device is USB-powered and always charging; the SY6970 status LED is a useful "it has power" signal on the bench and costs nothing. Trivially reversible if it turns out to be visually distracting next to a pure-black AMOLED clock.

**Related.** [[T4-S3]], [[Stage 1 - Static digits]]
