---
title: D010 - Night dimming is a schedule, not a sensor
type: decision
id: D010
date: 2026-08-16
status: accepted
origin: brief
tags:
  - decision
  - amoled
---

# D010 — Night dimming is a clock schedule, not a light sensor

**Decision.** Day/night brightness switches on **configured wall-clock hours**, not on ambient light.

**Why.** The `BOARD_AMOLED_241` descriptor has `sensor = NULL`. The `SensorCM32181` ambient-light sensor that `LilyGo_AMOLED` inherits is only wired on the 1.47" board (`AMOLED_147_SENSOR_PINS`). **The T4-S3 has no ambient light sensor** — there is nothing to read. The brief's day/night hours in `config.h` are the only mechanism available, and they are also the more predictable one for a desk clock.

**Consequence.** Brightness is a pure function of local hour, evaluated whenever the minute ticks. That means it also depends on NTP + timezone being correct; before the first sync the firmware uses the day level.

**Interaction with long-press.** A manual brightness change from the Stage 5 long-press wins until the **next day/night boundary crossing**, at which point the schedule takes over again. It does not persist across reboots — no NVS writes, keeping flash wear at zero for a device that is always on.

**Related.** [[D008 - Brightness scale]]
