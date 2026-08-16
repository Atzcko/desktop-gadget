# Log

Append-only record of project lifecycle, newest at the bottom. Format: `## [YYYY-MM-DD] type | title`.

## [2026-08-16] setup | Project instantiated

Empty directory → PlatformIO project + Obsidian vault. Vault root = project root, matching the convention used in the owner's other Projects folders (`index.md` / `log.md` / `CLAUDE.md` at root).

Environment survey found: no PlatformIO installed; system `python3` is **3.14.6** (too new for PlatformIO Core 6.x); the T4-S3 **already connected and enumerating** as `0x303A:0x1001` on `/dev/cu.usbmodem2101`. Installed PlatformIO Core 6.1.19 into a `python3.12` venv at `~/.platformio-venv` ([[D005 - PlatformIO runs on Python 3.12]]).

Flagged and mitigated: the project lives in **iCloud Drive**, which is hostile to a `.pio` build tree (sync storm, file eviction, latency). Build output redirected to `~/.pio-builds/`, library clone kept at `~/.local/src/` ([[D006 - Keep build artifacts out of iCloud]]).

## [2026-08-16] research | Ground truth from the library source

Shallow-cloned `Xinyuan-LilyGO/LilyGo-AMOLED-Series` v1.2.4 and read the source rather than the README. Findings that changed the plan:

- **LVGL 8.4.0 confirmed** in both `platformio.ini` and `library.json`. `LV_Helper.cpp` is wrapped in `#if LVGL_VERSION_MAJOR == 8` — under LVGL 9 it compiles to nothing and the link fails. ([[D002 - Pin LVGL to 8.4.0]])
- **There is only one board JSON** for the entire series, `T-Display-AMOLED.json`. Variant selection is a *runtime* call, not a build target. This is why our env is named `T-Display-AMOLED` despite targeting a T4-S3.
- **Rotation 0 is already 600×450 landscape.** `RM690B0_WIDTH = 600`, `RM690B0_HEIGHT = 450`, and `beginAMOLED_241()` ends with `setRotation(0)`. The expected "rotate to landscape" step does not exist. ([[D004 - Rotation 0 is already landscape]])
- **`beginLvglHelper()` already puts the framebuffer in PSRAM** — one full-screen 527 KB `ps_malloc` buffer. The brief's PSRAM requirement is satisfied by the stock path; the DMA variant would actually violate it. ([[D009 - LVGL buffer strategy]])
- **The T4-S3 has no ambient light sensor** (`sensor = NULL` in `BOARD_AMOLED_241`). Day/night dimming must be schedule-driven. ([[D010 - Night dimming is a schedule, not a sensor]])

Recorded 10 decisions and 6 stage notes.

## [2026-08-16] stage-0 | Stock example built, flashed, board confirmed

Built `examples/Factory` from the upstream repo with `platformio.ini` **completely unmodified**, per the brief's Stage 0 rule.

- Build SUCCESS, 3 m 33 s. RAM 19.9 % (65 168 / 327 680 B), Flash 38.1 % (2 498 001 / 6 553 600 B).
- Flash SUCCESS, 2 498 416 B at 963 kbit/s, hash verified.
- Serial: `Board Name:LilyGo AMOLED 2.41 inch` — auto-detect resolved to `LILYGO_AMOLED_241`.
- **Touch confirmed alive**: the library's `Failed to find CST226SE` error is *absent*, so the controller ACKed on I²C.
- Benign errors in the log: 1.47"/1.91" probe misses (that is how auto-detect works), no SD card inserted, and the example's placeholder Wi-Fi credentials.

Toolchain, board config, QIO flash / OPI PSRAM, USB CDC and panel bring-up are all proven. **Held at the Stage 0 gate** pending the owner's visual confirmation that the panel is lit and touch responds — those two cannot be established from a serial log.

## [2026-08-16] stage-1 | Fliqlo layout built and flashed

Owner confirmed the device was connected and asked for the firmware to be flashed, moving past the Stage 0 gate. Built the project's own firmware for the first time.

**Typography was the real problem.** LVGL 8's bundled Montserrat stops at 48 px and an `lv_label` cannot be scaled, so the ~150 px Fliqlo digits had nowhere to come from. Solved by generating a **digits-only 210 px LVGL font** with `lv_font_conv` — ten glyphs, 4 bpp, ~90 KB. It came back with a **tabular advance of 116.8 px identical across all ten digits**, which is what stops the clock from jittering as digits change, and every card dimension now derives from it. ([[D011 - Generate the digit font, do not scale Montserrat]], [[D012 - Card geometry]])

Discovered while reading `lv_conf.h`: the library already sets `LV_MEM_CUSTOM = 1` with `ps_malloc`, so **LVGL's object allocator is on PSRAM too**, not just the framebuffer. And Montserrat 12–48 are all enabled, covering the weather block. No changes to `lv_conf.h` were needed at all.

Build succeeded on the first attempt. RAM 6.8 % (22 140 B), Flash 12.7 % (834 041 B) — a third of Stage 0's image, because Wi-Fi/BLE/SD are not linked yet.

**Hardware confirmed three documented predictions:** `Panel : 600 x 450` with no `setRotation` call ([[D004 - Rotation 0 is already landscape]]); PSRAM fell by exactly **544 640 B** across `beginLvglHelper()` against a predicted 540 000 B ([[D009 - LVGL buffer strategy]]); `Touch : online`. Cold boot to ready in **1397 ms** against a 10 s budget.

Held for the owner's visual check of the layout — proportions, true-black background and digit legibility are not things a serial log can answer.
