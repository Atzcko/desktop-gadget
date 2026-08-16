# Desktop gadget — LilyGO T4-S3 flip clock

Firmware for a LilyGO T4-S3 (ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM, 2.41" AMOLED 450×600 over QSPI, capacitive touch) running a Fliqlo-style flip clock with weather and an HTTP emotion API.

**This directory is both the PlatformIO project and an Obsidian vault.** Start at [[index]]. Every decision lives in `docs/decisions/` with its reasoning; every stage in `docs/stages/`; the chronological record in `log.md`.

## Working agreements

1. **Read the decisions before changing anything structural.** `docs/decisions/` exists so the same ground is not re-litigated. If you disagree with one, write a new decision note superseding it — do not silently contradict it.
2. **Update `log.md` at the end of any session that changes the project.** Append-only, newest at the bottom.
3. **Stages are hardware-gated.** The owner flashes and verifies each stage on real hardware before the next begins. Do not run ahead of the gate.
4. **Ground truth is the library source, not the README.** `~/.local/src/LilyGo-AMOLED-Series`. Several README-level assumptions turned out wrong — see the 2026-08-16 research entry in `log.md`.

## Environment

PlatformIO lives in a Python 3.12 venv because the system Python is 3.14 ([[D005 - PlatformIO runs on Python 3.12]]). **Every** shell that runs `pio` needs this first:

```bash
export PATH="$HOME/.platformio-venv/bin:$PATH"
```

The board is at `/dev/cu.usbmodem2101` (`0x303A:0x1001`).

## Commands

Build:

```bash
export PATH="$HOME/.platformio-venv/bin:$PATH" && pio run
```

Flash:

```bash
export PATH="$HOME/.platformio-venv/bin:$PATH" && pio run -t upload --upload-port /dev/cu.usbmodem2101
```

Serial monitor:

```bash
export PATH="$HOME/.platformio-venv/bin:$PATH" && pio device monitor -p /dev/cu.usbmodem2101 -b 115200
```

Rebuild the Stage 0 stock example (from the upstream clone, never modified):

```bash
export PATH="$HOME/.platformio-venv/bin:$PATH" && cd ~/.local/src/LilyGo-AMOLED-Series && pio run -e T-Display-AMOLED
```

## Hard constraints

- **LVGL 8.4.0, pinned.** Not 9.x. `LV_Helper.cpp` is `#if LVGL_VERSION_MAJOR == 8`; upgrading breaks the link, not just the API. ([[D002 - Pin LVGL to 8.4.0]])
- **Do not hand-roll board settings.** The env is copied verbatim from the library. Every flag is load-bearing. ([[D003 - Copy the library platformio env verbatim]])
- **Do not use TFT_eSPI for the panel.** It has no QSPI transport. ([[D001 - Use the LilyGO AMOLED library, not TFT_eSPI]])
- **No BLE in v1.** That is the later HID phase.
- **No deep sleep, no battery logic.** USB-powered, always on.
- **`config.h` holds first-boot DEFAULTS ONLY.** Live configuration lives in NVS. Read `settings_get()`, never a `DEFAULT_*` macro, outside `settings_reset()`. ([[D013 - Settings live in NVS, config.h is only defaults]])
- **Wi-Fi credentials are never in a file.** They are typed on the device. Do not ask the owner for their Wi-Fi password. ([[D016 - Wi-Fi is provisioned on-device]])
- **Any new `lv_obj_create()` that is decorative must call `decor()`.** Every LVGL object is CLICKABLE by default and events do not bubble; a decorative object that keeps the flag will silently break the hold gesture. ([[D014 - Touch hit-testing]])
- **Never call LVGL from a non-LVGL task.** The weather task and the async web server handlers post to a queue; only the LVGL loop touches LVGL objects.

## Using the device

**Hold a finger anywhere on the screen for 5 seconds** to open Settings. A thin accent bar fills along the bottom edge from ~1.2 s so you can see the hold registering.

| Gesture | Action |
|---|---|
| Tap | date + weather sync age, 5 s |
| Long press 1.2–5 s | cycle brightness |
| Hold 5 s | Settings |

Settings tabs: **Wi-Fi** (scan / join), **Time** (zone, 24 h), **Place** (city search), **Screen** (brightness, night hours, toggles), **Info** (diagnostics, reset).

Settings persist in NVS and **survive re-flashing**. Only "Reset to defaults" clears them.

## Emotion API

> [!note] Not built yet
> This lands in [[Stage 4 - Emotion API]]. The curl examples and the standing permission for Claude to call the endpoint will be written here once the endpoint exists and is verified on hardware.
