---
title: Stage 0 - Stock example
type: stage
stage: 0
status: awaiting-hardware-verification
date_started: 2026-08-16
tags:
  - stage
---

# Stage 0 — Stock library example

**Goal.** Prove toolchain, panel init and touch on real hardware **before writing a single line of feature code**. Nothing here is ours.

## What is being built

The upstream repo at `~/.local/src/LilyGo-AMOLED-Series`, **entirely unmodified**:

- `src_dir = examples/Factory` (the repo default)
- `default_envs = T-Display-AMOLED` (the repo default)
- `platformio.ini` untouched

```bash
cd ~/.local/src/LilyGo-AMOLED-Series
export PATH="$HOME/.platformio-venv/bin:$PATH"
pio run -e T-Display-AMOLED
```

## Why `Factory` and not a T4-S3-named example

There is no example folder named for the T4-S3. The repo's T4-S3-specific examples (`PPM_Example_for_T4S3`, `LVGL_Rotation`, `TFT_eSPI_Sprite_Rotation`) each exercise **one** subsystem. `Factory` is the repo default and the broadest: it calls `begin()` (runtime auto-detect), brings up the panel, LVGL and touch, and draws an interactive UI. For a stage whose entire purpose is "does this board work at all", the broadest example is the right one. Auto-detect landing on `LILYGO_AMOLED_241` is itself part of what we want proven.

## Flash and verify

```bash
cd ~/.local/src/LilyGo-AMOLED-Series
export PATH="$HOME/.platformio-venv/bin:$PATH"
pio run -e T-Display-AMOLED -t upload --upload-port /dev/cu.usbmodem2101
pio device monitor -p /dev/cu.usbmodem2101 -b 115200
```

## Acceptance — what to look for

- [ ] Build completes without error
- [ ] Upload succeeds over `/dev/cu.usbmodem2101`
- [ ] Serial banner reports the board detected as the **2.41 inch / T4-S3** variant
- [ ] Panel lights up and renders the Factory UI
- [ ] **Touch responds** — the Factory UI reacts to taps
- [ ] No boot loop, no PSRAM allocation failure in the log

> [!note] Why this stage exists at all
> If the panel is dark or touch is dead after Stage 0, the fault is hardware, wiring, or toolchain — *never* our code, because there is no our-code yet. Every later stage gets to assume this baseline. Skipping it means debugging three unknowns at once.

## Outcome — 2026-08-16

**Build: SUCCESS** in 3 m 33 s.

```
RAM:   [==        ]  19.9% (used 65168 bytes from 327680 bytes)
Flash: [====      ]  38.1% (used 2498001 bytes from 6553600 bytes)
```

**Flash: SUCCESS** — 2 498 416 bytes written at 963 kbit/s, `Hash of data verified.`

**Serial boot log:**

```
ESP-ROM:esp32s3-20210327
rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)
[   461][E] begin(): Unable to detect 1.47-inch board model!
[   479][E] begin(): Unable to detect 1.91-inch touch board model!
[  2318][E] beginAMOLED_241(): Failed to detect SD Card!
============================================
    Board Name:LilyGo AMOLED 2.41 inch
============================================
init button : 0
[Error] : WiFi ssid and password are not configured correctly
```

### Reading the log

> [!success] `Board Name:LilyGo AMOLED 2.41 inch`
> Runtime auto-detect resolved to `LILYGO_AMOLED_241`. Toolchain, board JSON, QIO/OPI memory config and panel bring-up are all confirmed good.

The three `[E]` lines above it are **all expected**, not faults:

| Line | Why it is benign |
|---|---|
| `Unable to detect 1.47-inch` | `begin()` probes each variant in turn; this is a miss on the way to the match |
| `Unable to detect 1.91-inch touch` | same — auto-detect walking the list |
| `Failed to detect SD Card!` | no card in the slot. We pass `disable_sd = true` anyway, per [[D007 - Skip SD, keep charge LED default]] |
| `WiFi ssid and password are not configured` | the Factory example's placeholder credentials. Ours come from `config.h` |

> [!important] Touch is alive
> `beginAMOLED_241()` logs `Failed to find CST226SE - check your wiring!` when the touch controller does not ACK on I²C. **That line is absent**, so the CST226SE responded and touch is initialised. This is stronger evidence than a visual check alone.

### Still needs a human

Two things the serial log cannot prove:

- [ ] Panel is **visibly lit** and rendering the Factory UI
- [ ] Touch **responds to taps** (detected ≠ correctly mapped)

## Next

[[Stage 1 - Static digits]]
