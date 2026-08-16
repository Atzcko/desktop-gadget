---
title: D001 - Use the LilyGO AMOLED library, not TFT_eSPI
type: decision
id: D001
date: 2026-08-16
status: accepted
origin: brief
tags:
  - decision
  - display
---

# D001 — Use the LilyGO AMOLED library, not TFT_eSPI

**Decision.** All panel init, brightness control and touch input go through `LilyGo_AMOLED` from [[LilyGo AMOLED library]].

**Why.** The T4-S3's RM690B0 is driven over **QSPI**. TFT_eSPI has no QSPI transport and cannot address this panel. This was stated in the brief and is confirmed by the library source: the display is configured with `d0..d3`, `cmdBit = 8`, `addBit = 24` and pushed through `spi_device_handle_t` in `LilyGo_AMOLED.cpp`.

**Consequence.** `bodmer/TFT_eSPI @ 2.5.31` still appears in `lib_deps` because other examples in the library repo reference it. We keep it in the dependency list rather than pruning it, per [[D003 - Copy the library platformio env verbatim]].

**Status.** Accepted, not revisitable — it is a hardware fact.
