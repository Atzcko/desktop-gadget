---
title: D003 - Copy the library platformio env verbatim
type: decision
id: D003
date: 2026-08-16
status: accepted
origin: brief
tags:
  - decision
  - build
---

# D003 — Copy the library's PlatformIO env verbatim

**Decision.** Our `platformio.ini` reproduces the upstream `[env]` block exactly rather than hand-rolling board settings.

**The env, as upstream defines it:**

```ini
platform = espressif32@6.12.0
board = T-Display-AMOLED
framework = arduino
upload_speed = 921600
monitor_speed = 115200
build_flags =
    -DBOARD_HAS_PSRAM
    -DLV_CONF_INCLUDE_SIMPLE
    -DDISABLE_ALL_LIBRARY_WARNINGS
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCORE_DEBUG_LEVEL=1
boards_dir = boards
```

Plus, from `boards/T-Display-AMOLED.json`: `memory_type: qio_opi`, `flash_mode: qio`, `partitions: default_16MB.csv`, `-DARDUINO_USB_MODE=1`, `-DLILYGO_TDISPLAY_AMOLED_SERIES`.

**Why each flag is load-bearing** — these are not stylistic:

| Flag | What breaks without it |
|---|---|
| `-DBOARD_HAS_PSRAM` | `LilyGo_AMOLED.h` raises `#error "Detected that PSRAM is not turned on"` |
| `memory_type: qio_opi` | 8 MB OPI PSRAM is not mapped; `ps_malloc` of the 527 KB LVGL buffer fails |
| `-DARDUINO_USB_CDC_ON_BOOT=1` | `#warning` from the header; no serial output over native USB |
| `-DLV_CONF_INCLUDE_SIMPLE` | LVGL cannot find `lv_conf.h` |
| `-DLILYGO_TDISPLAY_AMOLED_SERIES` | board-family guard inside the library |

**Consequence.** `boards/T-Display-AMOLED.json` is **copied into this project** so `boards_dir = boards` resolves without depending on the clone location.

**Note.** The env name stays `T-Display-AMOLED` even though the target is a T4-S3 — there is only one board JSON for the whole series, and variant selection is a runtime call. See [[LilyGo AMOLED library]].
