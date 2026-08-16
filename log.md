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

## [2026-08-16] feature | Abu Dhabi locale + on-device settings, NTP, weather

Owner is in **Abu Dhabi** and asked for a 5-second hold to open a settings screen for timezone, city, brightness "and anything else interesting". That request reshaped the architecture more than it first appears.

**Timezone verified, not assumed.** Checked `Asia/Dubai` against system tzdata in both January and July: UTC+4, **no DST either way**, abbreviation `+04`. The POSIX string is `<+04>-4` — copied from tzdata, not composed. Two traps recorded in [[D015 - Abu Dhabi locale]]: the angle brackets are mandatory (numeric abbreviations are not parseable as POSIX names), and the offset sign is inverted (`-4` means UTC+4).

**`config.h` demoted to first-boot defaults.** A `#define` cannot be edited by a finger, so configuration moved to an NVS-backed `Settings` struct; macros were renamed `DEFAULT_*` so no call site can mistake one for live state. NVS survives re-flashing, so settings are not lost on firmware update. This partially supersedes [[D010 - Night dimming is a schedule, not a sensor]]'s "never persist" stance. ([[D013 - Settings live in NVS, config.h is only defaults]])

**Wi-Fi is now provisioned on the device** — scan, pick, type the password on the on-screen keyboard. `config.h` ships with empty credentials. The owner's password never enters a file, a chat, or a working tree. ([[D016 - Wi-Fi is provisioned on-device]])

**A real bug caught in review before flashing.** Reading `lv_obj_constructor` in LVGL 8.4.0: `obj->flags = LV_OBJ_FLAG_CLICKABLE` is set unconditionally on every object, and `LV_OBJ_FLAG_EVENT_BUBBLE` is *not* among the defaults. The cards, seams, colon dots and weather row would each have swallowed the press, so the 5-second hold would have worked **only in the 14 px margins** — a fault that presents as "the gesture is flaky" and is horrible to diagnose by hand. Fixed with a `decor()` helper applied at all 9 decorative call sites. ([[D014 - Touch hit-testing]])

**Also landed:** Stage 2 (non-blocking Wi-Fi, NTP, the two-phase split-flap fold), Stage 3 (weather on a dedicated FreeRTOS task with capped backoff and a stale dot), and Stage 5 (burn-in walk, tap and long-press) — the last brought forward because it shares the gesture plumbing.

**Log hygiene.** First implementation produced six `ESP_LOGE` lines on a healthy first boot, because Arduino's `Preferences` logs an error for every absent key even when a default is supplied. Six red herrings on a normal boot is what makes a real fault invisible later, so `settings_load()` now probes a sentinel key and seeds NVS instead of reading. Verified: both a virgin boot and a seeded boot emit zero error lines.

Build: RAM 15.5 % (50 720 B), Flash 22.8 % (1 493 757 B). UI up in **1565 ms**.

## [2026-08-16] feature | BLE, emotion API, and a Python client

Owner asked for BLE — device name configurable in Settings, discoverable from their Mac, driven by Claude to express what we are working on. That **reverses the brief's "v1 is Wi-Fi only, do NOT enable BLE"**, recorded as a deliberate change of direction in [[D017 - BLE and Wi-Fi coexistence]].

Built the emotion engine **once** and gave it two transports — BLE writes and HTTP POSTs share the same schema, parser and queue, so Stage 4 landed at the same time. ([[D018 - Emotion API - one engine, two transports]])

**Two boot loops, both radio coexistence, both diagnosed from decoded backtraces rather than guesses.**

1. `abort()` in `coex_core_enable ← coex_enable ← esp_bt_controller_enable ← NimBLEDevice::init`. The S3 has one 2.4 GHz radio; bringing up the BT controller **after** Wi-Fi has claimed it aborts. Fix: `ble_begin()` before `net_begin()`. This is now the most fragile ordering in the firmware and is commented as such.
2. `E wifi: Error! Should enable WiFi modem sleep when both WiFi and Bluetooth are enabled!!!!!!` — an error that names its own cause. `WiFi.setSleep(false)`, set deliberately for HTTP latency, is illegal once BT is on, because modem sleep is *how* Wi-Fi yields airtime. Now conditional on `ble_is_running()`.

**Wi-Fi was failing for an unrelated reason**, and the firmware was not saying why: it hammered `WiFi.reconnect()` from inside the event handler and logged a bare "disconnected, retrying". Added reason-code decoding and scheduled backoff, which produced the actual answer immediately — **reason 15 `4WAY_HANDSHAKE_TIMEOUT` then 202 `AUTH_FAIL`: the password is wrong**, SSID fine.

Before blaming the typing, checked `lv_textarea_get_text()` in the pinned LVGL: it returns `pwd_tmp`, the real text, not the bullets — so the firmware stored exactly what was typed. The fault is a mistyped password on a keyboard that showed no characters, which is a UX failure. Added a **"Show password" checkbox** and a **1 Hz status poll** so the Wi-Fi tab now reports "Failed: AUTH_FAIL (wrong password)" instead of "Connecting…" forever.

**NVS gained a general fix.** The first-boot sentinel handled a virgin device but not a key added by a *newer firmware* — `blen` (BLE name) put an error line back into a healthy boot log. Reads are now guarded by `isKey()` and a one-shot backfill runs when anything was missing, so adding a setting in future is a non-event.

Wrote `tools/flipclock.py` (HTTP **or** BLE, auto-falling-back, also importable) and the `CLAUDE.md` emotion section including the standing permission for Claude to drive the clock unprompted.

**Could not verify BLE from this session:** a scan from the agent's sandboxed shell dies with **SIGABRT (exit 134)** the moment CoreBluetooth is touched — macOS TCC denies Bluetooth by killing the process rather than returning an error. Needs to be run from the owner's own Terminal.

Build: RAM 18.1 % (59 288 B), Flash 26.9 % (1 762 537 B). UI up in 1651 ms, BLE advertising as "FlipClock".

## [2026-08-16] fix | Clock was draggable; hold shortened to 3 s

Owner reported the clock and weather block could be **dragged around with a finger**, and asked for the Settings hold to come down from 5 s to 3 s.

The drag was the sibling of the bug fixed in [[D014 - Touch hit-testing]] — same root cause, different flag. `lv_obj_constructor` sets `LV_OBJ_FLAG_SCROLLABLE` on every object and `LV_OBJ_FLAG_SCROLL_CHAIN` on every object with a parent. `decor()` cleared the former but not the latter, and more importantly **`lv_scr_act()` was never touched at all** — it is easy to think of the screen as a backdrop rather than an object, but it is an object with every default flag and it is the last stop on the scroll chain. A drag the root did not handle was forwarded up and scrolled the screen, moving the whole layout.

Fixed in both places: `decor()` now clears `SCROLL_CHAIN`, and `ui_init()` explicitly clears `SCROLLABLE` on the clock screen and turns its scrollbar off. Nothing on the clock screen is ever meant to scroll; the only thing that legitimately moves the layout is the burn-in walk, which sets a position directly.

`SETTINGS_MS` 5000 → 3000. The hold-progress bar derives its fill span from that constant, so the indicator stayed honest without a second edit. Brightness cycle now occupies the 1.2–3.0 s window; holding through to Settings still suppresses it, because `settings_fired` short-circuits the release handler.

Wi-Fi is still failing `AUTH_FAIL` — the stored password remains wrong pending the owner re-entering it with the new Show-password toggle.

## [2026-08-16] fix | Keyboard overhaul; API verified end-to-end

Owner reported the keyboard covered the field being typed into, and that once dismissed it could not be recovered without switching tabs. Both had one cause: the keyboard was a floating panel over the tab content, shown from **`LV_EVENT_FOCUSED`** — an edge event that fires only when focus is *gained*, so a re-tap on an already-focused field did nothing.

Replaced with a **dedicated full-screen editor** (title, large 24 px field at the top, Hide/Cancel/Done, keyboard below) opened on `LV_EVENT_CLICKED`. The field can no longer be occluded because its position is fixed and independent of tab layout, and there is no hidden/shown state to get stuck in. Passwords default to *visible* in the editor — the point is to see what you are typing. ([[D019 - Text entry gets its own screen]])

**BLE advertising fixed.** The 31-byte advertising payload was being consumed by the 128-bit NUS service UUID (18 bytes), so NimBLE silently demoted the name to the scan response and passive scanners saw "(unnamed)". Name now goes in the primary advertisement, service UUID in the scan response, plus a Generic Clock appearance (0x0100) and a 100–200 ms interval.

**The device came online during this session.** Owner re-entered the Wi-Fi password and renamed the device to "Flip Clock":

```
[net] IP 192.168.0.181  RSSI -54
[http] http://flipclock.local/
[net] weather 32.0 (30.5..40.7)
```

Verified from the Mac: `flipclock.local` resolves (`dns-sd` lists the `_http._tcp` service), `GET /health` returns full diagnostics with `time_synced: true`, `POST /emotion` returns `{"ok":true,...}`, and all three bad-input cases return **400 with a reason and no reboot**.

**One real bug found by that test:** the unknown-state error interpolated the offending value in double quotes, producing `{"ok":false,"error":"unknown state "banana" (...)"}` — **invalid JSON**, so a client trying to read the error would fail to parse it. Switched the message to single quotes and added a defensive `json_escape()` in `send_err()`, since the message can quote arbitrary user input. Re-verified: the error body now parses.

Minor known oddity: one spurious `AUTH_FAIL` on the first association attempt at boot, which self-corrects ~2 s later and then connects. Time-to-IP is unaffected; not chased.

## [2026-08-16] feature | Humidity, shown quietly

Owner asked for relative humidity, "discrete". The type-size budget was already spent — the brief caps the design at two sizes besides the clock digits, and 48 px (current temp) and 28 px (min/max) are both taken. Adding a third size would break the rule and start the slide toward a dashboard.

So humidity uses the **existing 28 px size** and recedes by **contrast**: `#4E4E4E` against min/max's `#8A8A8A`, roughly half the luminance, with a 14 px left pad so it reads as its own fact rather than a third temperature. On a true-black AMOLED a dim grey costs almost no light and never competes with the white digits. ([[D020 - Humidity recedes by contrast, not size]])

Fetched from Open-Meteo's `relative_humidity_2m`. A missing value defaults to `-1` and renders as nothing rather than a confident `0%` — a wrong reading is worse than an absent one. Toggle added under Settings ▸ Screen; `temp_c` and `humidity_pct` also added to `GET /health`.

Verified live: `[net] weather 32.7 (30.5..40.7) rh 81%`, and `/health` returns `"humidity_pct": 81`.

## [2026-08-16] feature | BLE HID, so macOS will actually list it

Owner: "I want to be able to see and connect to the device via Bluetooth settings."

The earlier answer — that a GATT peripheral cannot appear there — was correct but incomplete, because there *is* a way: **be a HID device**. macOS System Settings ▸ Bluetooth lists only devices implementing a profile it can pair with. Fixing the advertising payload made the clock show up properly in BLE *scanners*, but scanners are not System Settings.

So the device now advertises a **BLE HID keyboard** service alongside NUS. Appearance `0x03C1` and the 16-bit HID UUID `0x1812` both go in the primary advertisement — macOS needs both to classify it as a pairable input device — while the 128-bit NUS UUID moves to the scan response, because at 18 bytes it cannot share a 31-byte packet with them. Total primary payload: 23 bytes.

**It is a keyboard that never types.** No HID reports are ever sent; the service is purely the ticket into the Bluetooth list. That makes this a deliberate half-step into the HID phase the brief deferred — the plumbing is in place if real key or media reports are wanted later. ([[D021 - BLE HID, for discoverability not typing]])

Pairing is mandatory because HID characteristics must be encrypted. Just Works, no passkey: MITM protection would mean displaying a 6-digit code mid-pairing (hijacking the clock face) to protect a device that transmits nothing.

Two escape hatches in Settings ▸ BLE: a **Pairable** switch that reverts to GATT-only, and **Clear pairings** (`deleteAllBonds()`) — bonding state lives on both ends, and a half-forgotten pairing where the Mac remembers a bond the device has dropped is a classic wedge.

Known wart, documented rather than fixed: macOS may open Keyboard Setup Assistant on first pair. It cannot be answered by a device with no keys; closing it does not affect the pairing.

Cost: +3 KB flash. Verified advertising: `[ble] advertising as "Flip Clock" (HID keyboard — pairable from Bluetooth settings)`.

## [2026-08-16] fix | Keyboard off-screen; Save & close now saves Wi-Fi

Owner sent a photo of the editor with only the **top two keyboard rows** visible, crushed against the bottom edge — `z x c v b n m` and the space bar gone entirely.

Self-inflicted, and a good LVGL lesson. `lv_keyboard_constructor` ends with `lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, 0)`, and **in LVGL 8 alignment is a persistent style property**. Once an object is aligned, `lv_obj_set_pos()` stops meaning "put it here" and becomes an *offset from the alignment point*. So `lv_obj_set_pos(kb, 0, 162)` shoved the keyboard 162 px below the bottom of the screen. The previous shared keyboard used `lv_obj_align(...)` and was correct; the regression came from rewriting it with `set_pos`. Fixed by aligning it properly, and the rule is now in [[CLAUDE]]. ([[D019 - Text entry gets its own screen]])

Owner also asked that Wi-Fi and other settings persist across reboot. They already did — but there was one real gap: **credentials were committed only by the Connect button**, so picking a network, typing a password and tapping "Save & close" silently discarded both. `close_cb()` now commits them alongside everything else and re-associates if they changed. ([[D016 - Wi-Fi is provisioned on-device]])

Added `settings_dump()`, printed on boot and after every save, so persistence is *auditable from the log* rather than assumed. It immediately confirmed the rest was already working — after a full reflash:

```
[settings] restored from NVS
           ssid="Aleks Spontano" pass=(set)
           bright day=251 night=25  night 21:00-07:00
           ble=1 hid=1 name="Flip Clock"
```

`day=251` and `night 21:00` are not defaults (90 and 22) — they are the owner's own edits surviving a firmware update, which is exactly the intended NVS behaviour from [[D013 - Settings live in NVS, config.h is only defaults]].

**BLE HID pairing confirmed working**: `[ble] client connected`, and `/health` reports `"ble":{"running":true,"connected":true,"name":"Flip Clock"}`.

## [2026-08-16] feature | Custom HID identity — a gadget, not a keyboard

Owner: "make it a custom HID device, so my Mac knows it's not a keyboard, it's a desktop gadget."

[[D021 - BLE HID, for discoverability not typing]] got into the Bluetooth list by pretending to be a keyboard. That worked but lied about the device, and was the direct cause of the Keyboard Setup Assistant wart.

Three layers had to change together, and the important one is **the report descriptor** — appearance only picks an icon, the descriptor is what the OS parses to decide what the device *is*:

| Layer | Was | Now |
|---|---|---|
| Report descriptor | Generic Desktop / Keyboard | vendor page `0xFF00`, usage `0x01` |
| Appearance | `0x03C1` Keyboard | `0x03C0` Generic HID |
| DIS model string | absent | "Flip Clock Desktop Gadget" |

A vendor-defined usage page has no OS-level meaning, so macOS enumerates a generic HID device and cannot mistake it for text input. **Keyboard Setup Assistant should no longer appear** — that dialog only fires on an unknown keyboard.

The descriptor is not decorative: two 32-byte reports, and the **output report is wired to the emotion engine** with a compact binary command (`0xE0`, state, duration LE, message), validated through the same parser and queue as NUS and HTTP. Honest limitation recorded: once macOS claims a HID device, userspace cannot easily write output reports to it, so NUS/HTTP remain the transports to actually use — the HID command path is there for completeness. ([[D022 - Custom HID identity, not a keyboard]])

Kept the keyboard descriptor behind `Settings ▸ BLE ▸ Identify as keyboard (fallback)`, default off. macOS is guaranteed to list a keyboard; a purely vendor-defined descriptor is less certain and varies by OS version, so both identities ship and a switch chooses rather than gambling on one.

**Documented the trap that will otherwise waste an hour:** hosts cache the HID descriptor *per bond*. Flipping the switch on an already-paired device changes nothing until you Forget on the Mac, Clear pairings on the clock, and pair again.

The new `bkbd` NVS key was picked up silently by the `isKey()` backfill added earlier — `[settings] backfilling keys added by a newer firmware`, then a clean boot. Verified: `advertising as "Flip Clock" — custom HID desktop gadget`, `as_keyboard=0`.
