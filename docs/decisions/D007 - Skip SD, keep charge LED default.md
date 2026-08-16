---
title: D007 - Skip SD, silence the charge LED
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

## Reversed 2026-08-16 — `disable_state_led = true`

The original reasoning was that the SY6970 status LED is a useful "it has power" signal on the bench, and that it would be *"trivially reversible if it turns out to be visually distracting next to a pure-black AMOLED."* It did, and it was.

**It does not report power — it reports a fault.** There is no battery, so the charger can never complete a cycle, and a blinking red LED is exactly how the SY6970 says so. The signal is correct; it describes a condition that will never change and that nobody needs to know about.

On a desk object whose entire point is a black panel with pixels genuinely off, a red light flashing forever on the back is the only thing in the room that looks broken. `SY.disableStatLed()` silences it, and charging is unaffected because there is nothing to charge.

**Related.** [[T4-S3]], [[Stage 1 - Static digits]]
