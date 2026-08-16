---
title: LilyGo AMOLED library
type: reference
source: https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series
version: 1.2.4
tags:
  - reference
  - library
updated: 2026-08-16
---

# LilyGo-AMOLED-Series — what we actually use

Cloned shallow to `~/.local/src/LilyGo-AMOLED-Series` (400 MB — datasheets, schematics and dimension drawings dominate; the code is small). Kept **outside** iCloud on purpose, see [[D006 - Keep build artifacts out of iCloud]].

## Why this library and not TFT_eSPI

The RM690B0 on the T4-S3 is a **QSPI** panel. TFT_eSPI has no QSPI transport, so it cannot drive this display at all — the `TFT_eSPI_*` examples in this repo target the other boards in the series or run through the library's own compatibility shim. Panel init, brightness and touch all go through `LilyGo_AMOLED`. This was a given in the brief and the source confirms it.

## The one board definition

There is a **single** board JSON — `boards/T-Display-AMOLED.json` — shared by every board in the series. There is no `T4-S3` board file. Variant selection happens **at runtime**, not at build time:

- `begin()` auto-detects by probing I²C addresses, or
- `beginAMOLED_241(...)` selects the T4-S3 explicitly.

This is why the PlatformIO env is named `T-Display-AMOLED` even though we are targeting a T4-S3.

## Public API we depend on

```cpp
LilyGo_Class amoled;

amoled.beginAMOLED_241(/*disable_sd=*/true, /*disable_state_led=*/false);
amoled.setBrightness(level);   // 0..255
amoled.getBrightness();
amoled.width();                // 600 at rotation 0
amoled.height();               // 450 at rotation 0
amoled.getPoint(&x, &y, 1);    // returns touch count
amoled.hasTouch();
amoled.setRotation(r);
amoled.readCoreTemp();
```

`AMOLED_DEFAULT_BRIGHTNESS` is `175`. The brief asks for ~35 % of max — see [[D008 - Brightness scale]].

## LVGL glue — `beginLvglHelper()`

`src/LV_Helper.cpp` is compiled only when `LVGL_VERSION_MAJOR == 8`. It does all of this for us:

- `lv_init()`
- allocates **one full-screen buffer** with `ps_malloc()` → `600 × 450 × 2 = 540 000 B ≈ 527 KB` **in PSRAM**
- registers `disp_flush` → `board.pushColors(x, y, w, h, buf)`
- registers the touch indev with `touchpad_read` when `board.hasTouch()`
- installs a `rounder_cb` that snaps every flush area to **even coordinates** on all four edges

> [!warning] The rounder callback is not cosmetic
> RM690B0 wants even-aligned windows. Any custom flush path must preserve this alignment or the image tears/shifts. We use the stock helper rather than rolling our own display driver.

There is also `beginLvglHelperDMA()` which allocates two `1/10`-screen buffers with `MALLOC_CAP_DMA` (~54 KB each of *internal* SRAM). Faster flush, but it spends scarce internal RAM. Decision recorded in [[D009 - LVGL buffer strategy]].

## Dependency set (from the library's own `platformio.ini`)

```ini
platform = espressif32@6.12.0
framework = arduino
lvgl/lvgl @ 8.4.0
lewisxhe/XPowersLib @ 0.2.7
lewisxhe/SensorLib @ 0.2.4
bodmer/TFT_eSPI @ 2.5.31
bxparks/AceButton @ 1.10.1
```

The header hard-errors if `SENSORLIB_VERSION_MAJOR` is undefined or PSRAM is off, and warns if `ARDUINO_USB_CDC_ON_BOOT != 1`. These are load-bearing — see [[D003 - Copy the library platformio env verbatim]].

## `lv_conf.h`

The library ships its own `src/lv_conf.h` (and a `.v9` variant it does not use at LVGL 8). It is picked up because the env sets `-DLV_CONF_INCLUDE_SIMPLE`. Our project needs its own copy on the include path — tracked in [[Stage 1 - Static digits]].

## Related

- [[T4-S3]]
- [[Stage 0 - Stock example]]
