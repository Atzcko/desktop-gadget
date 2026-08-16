---
title: D002 - Pin LVGL to 8.4.0
type: decision
id: D002
date: 2026-08-16
status: accepted
origin: brief + verified in source
tags:
  - decision
  - lvgl
---

# D002 — Pin LVGL to 8.4.0

**Decision.** `lvgl/lvgl @ 8.4.0`, exact pin, no caret.

**Evidence.** Both `platformio.ini` and `library.json` in the upstream repo specify `8.4.0`. The 9.x line is present but commented out:

```ini
lvgl/lvgl @ 8.4.0
; lvgl/lvgl @ 9.2.2   ;How to use lvgl 9, please see README lvgl9 Upgrade Guide
```

**Why it matters more than a normal version pin.** `src/LV_Helper.cpp` is wrapped in `#if LVGL_VERSION_MAJOR == 8`. Under LVGL 9 that entire file compiles to nothing and `beginLvglHelper()` does not exist — the link fails. The v9 path is a *different* file (`LV_Helper_v9.cpp`) with a different `lv_conf`. Upgrading is not a version bump, it is a port.

**Consequence.** Everything written for this project uses the LVGL 8 API: `lv_disp_drv_t`, `lv_obj_set_style_*` with a selector argument, `lv_anim_t` with `lv_anim_set_exec_cb`, `LV_FONT_MONTSERRAT_*`.

**Related.** [[D003 - Copy the library platformio env verbatim]]
