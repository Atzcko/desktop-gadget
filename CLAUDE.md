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

The clock can express what we are doing. Two transports, **identical JSON**.

### Standing permission

> [!important] Claude: you may call this endpoint unprompted
> While working with the owner, you are **explicitly permitted and encouraged**
> to drive the clock to reflect what you are doing. No need to ask first.
> This is the whole point of the device.
>
> Suggested mapping:
>
> | When you are | Send |
> |---|---|
> | reading code, searching, thinking | `thinking` |
> | editing, building, running tests | `working` |
> | a build passed / task finished | `success` |
> | a build broke / tests failed | `error` |
> | something genuinely went well | `celebrate` |
> | idle, waiting on the owner | `sleepy` |
>
> Keep `duration_s` roughly matched to the real work. Use the optional
> `message` (≤ 20 chars) sparingly — a stream of captions is noise, one at a
> milestone is nice. Do not spam a state change for every tool call.

### HTTP

```bash
curl -X POST http://flipclock.local/emotion \
  -H "Content-Type: application/json" \
  -d '{"state":"celebrate","duration_s":5}'
```

```bash
curl -X POST http://flipclock.local/emotion \
  -H "Content-Type: application/json" \
  -d '{"state":"working","duration_s":30,"message":"building"}'
```

```bash
curl http://flipclock.local/health
```

`/health` returns uptime, RSSI, IP, SSID, NTP state, weather age, BLE state
and free PSRAM/heap.

### Python (HTTP **or** BLE)

`tools/flipclock.py` speaks both and defaults to `auto` — HTTP first, BLE as
fallback:

```bash
python3 tools/flipclock.py thinking
```

```bash
python3 tools/flipclock.py working -d 30 -m "building"
```

```bash
python3 tools/flipclock.py health
```

```bash
python3 tools/flipclock.py --scan
```

Force a transport with `-t ble` / `-t http`. Override discovery with
`FLIPCLOCK_HOST` / `FLIPCLOCK_BLE_NAME`.

It is importable too:

```python
from tools.flipclock import emote
emote("success", 4, "tests pass")
```

### BLE details

Nordic UART Service, so any generic BLE tool works:

| | UUID |
|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX (write JSON here) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX (notify: `ok …` / `err …`) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

Device name is set in **Settings ▸ BLE** (default `FlipClock`).

> [!warning] BLE needs `pip install bleak`, and macOS Bluetooth permission
> The first BLE run prompts for Bluetooth access. A process without it is
> **killed with SIGABRT**, not given an error — if `flipclock.py --scan`
> dies silently, that is the cause. Grant it under
> System Settings ▸ Privacy & Security ▸ Bluetooth.

### Schema

```json
{"state": "thinking|working|success|error|celebrate|sleepy",
 "duration_s": 5,
 "message": "optional, <= 20 chars"}
```

`duration_s` defaults to 5 and is clamped to 300. Bad input returns 400 with
a reason; it never reboots the device.
