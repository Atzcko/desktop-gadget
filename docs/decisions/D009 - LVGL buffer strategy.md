---
title: D009 - LVGL buffer strategy
type: decision
id: D009
date: 2026-08-16
status: accepted
origin: analysis
tags:
  - decision
  - lvgl
  - memory
---

# D009 — Full-screen PSRAM buffer via `beginLvglHelper()`

**The choice.** The library offers two LVGL bring-up paths:

| | `beginLvglHelper()` | `beginLvglHelperDMA()` |
|---|---|---|
| Buffers | 1 × full screen | 2 × 1/10 screen |
| Size | 600×450×2 = **527 KB** | 2 × ~54 KB = **108 KB** |
| Where | `ps_malloc` → **PSRAM** | `MALLOC_CAP_DMA` → **internal SRAM** |
| Flush | `pushColors(x,y,w,h,buf)` | `setAddrWindow` + `pushColorsDMA` |

**Decision.** Use `beginLvglHelper()`.

**Why.**

1. **The brief requires framebuffers in PSRAM.** The DMA path puts them in internal SRAM by definition — `MALLOC_CAP_DMA` on the ESP32-S3 cannot be satisfied from OPI PSRAM.
2. **Internal SRAM is the scarce resource here, not PSRAM.** The S3 has 320 KB of internal RAM total, and this project spends it on the Wi-Fi stack, ESPAsyncWebServer, mDNS, and three FreeRTOS task stacks. Handing 108 KB of it to display buffers to speed up a clock that redraws twice a minute is the wrong trade. We have 8 MB of PSRAM and use 527 KB of it.
3. **A single full-screen buffer is the anti-tearing configuration** for the split-flap animation. LVGL renders the whole frame into one buffer and flushes it in one `pushColors` call; there is no partial-frame window where the panel shows half of frame *n* and half of frame *n+1*.

**Accepted cost.** PSRAM reads are slower than SRAM, so full-screen flushes are not free. Budget check: 540 000 B over a 36 MHz QSPI bus (4 bits × 36 MHz = 18 MB/s theoretical) is **~30 ms minimum** per full flush. At 8–10 animation frames that is a ceiling around 30 fps for a full-screen redraw — comfortably enough for a ~400 ms flip, and the flip only dirties the two card rectangles, not the whole screen.

> [!warning] Do not fight the rounder callback
> `beginLvglHelper()` installs `lv_rounder_cb`, which snaps every flush area to even coordinates. The RM690B0 needs this. Card geometry and the burn-in jitter from Stage 5 should prefer even pixel values so LVGL's rounding never enlarges a redraw region unexpectedly.

**Revisit if.** The flip animation visibly stutters on hardware at Stage 2. The fallback is not the DMA helper (it breaks the PSRAM requirement) but reducing the animated area.
