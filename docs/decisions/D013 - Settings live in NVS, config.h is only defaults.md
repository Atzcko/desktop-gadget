---
title: D013 - Settings live in NVS, config.h is only defaults
type: decision
id: D013
date: 2026-08-16
status: accepted
supersedes: partially D010
origin: owner request
tags:
  - decision
  - settings
---

# D013 — Settings live in NVS; `config.h` is only first-boot defaults

**Trigger.** The owner asked for a 5-second hold to open an on-device settings screen for timezone, city, brightness "and anything else interesting". That is incompatible with configuration living in `#define`s: a compile-time constant cannot be edited by a finger.

**Decision.** Introduce a `Settings` struct persisted in **NVS** (`Preferences`, namespace `flipclock`).

- `config.h` supplies **first-boot defaults only**. Its macros were renamed `DEFAULT_*` to make that unambiguous at every call site.
- Everything in the firmware reads `settings_get()`. Nothing reads `config.h` except `settings_reset()`.
- **NVS survives re-flashing.** Settings are not lost on firmware update; only "Reset to defaults" clears them.

**Supersedes part of [[D010 - Night dimming is a schedule, not a sensor]]**, which said brightness changes would never be persisted to keep flash wear at zero. That was the right call when the only mechanism was a long-press cycle. Now that there is a real settings screen, persistence is the expected behaviour. Wear is still a non-issue: `Preferences` skips the write when the stored value is unchanged, and a human edits settings a handful of times in the device's life.

## The first-boot log noise

The first implementation opened the namespace read-only, which on a virgin device produced:

```
[E][Preferences.cpp:50] begin(): nvs_open failed: NOT_FOUND
```

Opening read-write fixed that but exposed a worse one — Arduino's `Preferences` logs `ESP_LOGE` for **every absent key**, even when a default is supplied:

```
[E][Preferences.cpp:483] getString(): nvs_get_str len fail: ssid NOT_FOUND
[E][Preferences.cpp:483] getString(): nvs_get_str len fail: pass NOT_FOUND
... six lines on a perfectly healthy boot
```

> [!warning] Error-level noise on a healthy path is not cosmetic
> Six red herrings on every first boot is exactly what makes a real fault invisible later. A log you have learned to ignore has stopped being a log.

**Fix:** probe one sentinel key (`tz`). Absent → this is a first boot, so seed NVS from the defaults and return without reading. Present → the namespace is fully populated and every read is silent. Verified: both a virgin boot and a seeded boot now produce zero error lines.

**Related.** [[D014 - Touch hit-testing]], [[Settings screen]]
