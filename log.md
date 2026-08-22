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

## [2026-08-16] verification | Acceptance run

Ran the brief's four acceptance criteria. Full detail in [[Acceptance results]].

**1. Cold boot → correct time in under 10 s: PASS at 4.45 s.** Measured from hardware reset by correlating the serial log with `/health` polling — UI at 1.64 s, IP at 2.46 s, NTP valid at 4.45 s. 55 % headroom.

**3. Emotion animates then reverts: PASS at 5.2 s.** Added `emotion.active` / `remaining_s` to `/health` so this is observable rather than a matter of faith. Validation also passes: unknown state, missing state and over-long message all return 400 with a reason; `duration_s: 99999` clamps to 300; uptime rose across the whole suite, so nothing rebooted.

> A false negative worth remembering: the first run of the revert test reported FAIL. The fault was the **test**, not the firmware — it polled `flipclock.local`, and re-resolving mDNS per request made each sample slow enough to step over the entire 5 s window. Against the IP it passed immediately. Measure through the cheapest path available, or you end up debugging your own instrument. (An earlier failure in the same session was also self-inflicted: a curl and a serial reset issued in parallel, so the reset landed mid-request.)

**4. Wi-Fi resilience: PASS, from field evidence** rather than a staged test — the wrong-PSK episode earlier in the project was a genuine multi-minute outage. The clock kept rendering, weather held its last values and raised the stale marker, backoff capped at 30 s, and recovery needed no intervention.

**2. Minute and midnight rollover: still needs the owner's eyes.** It cannot be automated without adding a debug endpoint to production firmware. Code review confirms the property that matters: the flip is driven by "rendered digits differ from target, per card", never by arithmetic on the previous value — which is exactly what makes 23:59 → 00:00 fold correctly instead of treating `00` as a decrement.

## [2026-08-16] fix | Emotions were invisible — a design failure, not a bug

Owner: "when you do that it keeps displaying time and I can't see emotions."

Added a serial diagnostic before touching any rendering code, which settled the question immediately:

```
[ui] emotion 5 rendered: eyes=yes seam=on digits=dimmed msg=yes
```

The overlay was always being built. This was never a rendering bug — it was a **design failure**. For four of the six states (`thinking`, `working`, `success`, `error`) the entire visual signal was a **3 px accent line** on the card seam, competing with 150 px white digits at brightness 251. Invisible, obviously, in hindsight.

The brief asked for minimalist animations and I read that as *quiet*. Wrong reading: **minimal means few elements, not low contrast**. An expression nobody notices has failed completely.

Rebuilt so the device visibly changes **mode** rather than gaining decoration: clock digits drop to 30 % opacity, both cards gain a 4 px border in the state colour, the seam accent goes 3 px → 10 px and pulses, **every** state now shows eyes (previously only celebrate and sleepy), and the caption goes 20 px → 28 px in the state colour. Eyes carry per-state motion — glancing for thinking, blinking for working, bouncing for success/celebrate, shaking for error, breathing bars for sleepy — because motion reads at a glance far better than shape.

The resting clock is untouched: the accent bar is fully transparent and the border zero-width at rest, so the default Fliqlo face is still exactly what the brief specifies. ([[D023 - Emotions must change the mode, not decorate it]])

Second time in this project a *design* failure presented as a *code* failure — the other was the invisible 5-second hold before a progress bar was added. Both times the mechanism was perfect and the human could not tell. The cheap `Serial.printf` that distinguishes "broken" from "invisible" paid for itself in one flash cycle.

## [2026-08-16] design | v3 emotions — the neon wave

Owner's photo of v2 settled it: the eyes rendered as **pale blobs colliding with the numerals**. Visible, but ugly and nothing like the product's aesthetic. Their redirection was better than the fix I would have reached for:

> "When I say emotions it doesn't have to be with eyes. It can be something like the 80s vibe line that moves and changes color."

A line is the right shape for this problem. It does not fight the digits for the same real estate the way an eye-shaped blob does — it owns a band, and the clock reads straight through it.

So: a synthwave sine line running **through the card seam**, 30 segments at ~30 fps, each with a dim oversized pass underneath faking a neon bloom (the panel has no glow; on true black it reads convincingly). Hue is computed per segment in HSV and rotated over time, so the line genuinely shifts colour rather than switching between fixed ones. Per-state *motion* carries the meaning: gentle for thinking, tight and quick for working, a broad swell for success, hard squared-off spikes for error, full-spectrum and fast for celebrate, barely-moving violet for sleepy. Eyes and card borders are gone; digits stay at 30 %.

**The decision that made 30 fps possible:** it is ONE object with a custom `LV_EVENT_DRAW_MAIN` callback, not 30 moving objects. Thirty movers would queue up to 60 invalidated rects per frame; overflow `LV_INV_BUF_SIZE` and LVGL abandons partial redraw and repaints the whole screen — 540 KB/frame, 16 MB/s at 30 fps against a ~18 MB/s QSPI ceiling. Custom-drawing into a single object gives exactly one invalid region: the 600×96 band, ~115 KB/frame, ~3.4 MB/s. ([[D023 - Emotions must change the mode, not decorate it]])

Third design for this feature. The first two failed for the same underlying reason — I was designing decoration instead of asking what the owner would actually *see* from across a desk.

## [2026-08-16] design | v4 — the circumplex, and a line that is actually a line

Owner asked for three things: move the clock and weather aside during an emotion and animate the transition, research how many emotions exist, and render them on a single line — pointedly *not* the dashed thing v3 produced.

**The research changed the design.** Ekman says 6, Plutchik 8, Cowen & Keltner 27 — but Cowen & Keltner's real claim is that the categories are **"bridged by continuous gradients"**, not discrete. Russell's 1980 circumplex says the same thing constructively: every emotion is a point on two axes, **valence** and **arousal**.

That is exactly two free parameters, and a line has exactly two obvious ones:

- **arousal → agitation** (amplitude, frequency, speed)
- **valence → hue** (red at −1, violet-blue at 0, cyan-green at +1 — the synthwave palette read straight off the x-axis)

So the renderer implements the **space**, not a list. 26 named emotions share one renderer; adding another is one row in a table, never new drawing code. A `character` field (smooth / jagged / tremor / scan / droop) layers shape on top, because valence and arousal alone cannot tell *angry* from *afraid* — both are high-arousal negative.

**Why v3 looked dashed:** it drew 30 rectangles with gaps. v4 draws a polyline — 49 points, 48 `lv_draw_line` segments with round caps so the joints overlap — plus a wide dim pass under a narrow bright one to fake neon bloom.

**The clock now steps aside**, animated: big cards fade out and slide up, a 48 px clock fades in top-left, weather slides to top-right, and the line fades in at centre. The 210 px digit font cannot be scaled (LVGL 8 has no usable text transform), so the corner clock is a separate label kept in step rather than a shrunken object. Entrance/exit gain is ramped inside the wave timer rather than by `lv_anim`, so a rapid emotion change can never leave two animations fighting over one gain.

Fourth design for this feature. Each failure was informative: invisible → ugly → dashed → this. ([[D024 - Emotions as a circumplex, rendered as one line]])

## [2026-08-16] design | v5 — white, thin, and talking instead of oscillating

Owner: "make the line white and thin", and "now it's just a sinusoid, I was thinking more of a dynamically moving line mimicking talking, thinking".

The second note is the substantive one. **A single sine is periodic and symmetric, so it reads as a graph** — speech does not look like that. Three changes fix the reading:

1. **Additive harmonics at incommensurate ratios** (1 : 2.27 : 4.13). Irrational ratios mean the sum never repeats on screen, so the motion looks organic rather than looped.
2. **A speech envelope over time** — two slow oscillators at unrelated rates multiplied together, producing bursts and pauses the way talking does instead of a constant-amplitude drone. Its depth scales with arousal, so calm states barely modulate and activated ones burst hard.
3. **A taper across x**, `sin(pi*u)^0.75`, exactly zero at both ends. This is the biggest single cue — a stroke running edge to edge is a chart; one that swells in the middle and dies into black is a voice.

Stroke is now pure white: a 2 px bright core over a 9 px dim pass. The dim pass is not decoration — it is what stops a thin white line from reading as a rendering artefact on a black AMOLED. Valence consequently expresses itself entirely through motion character (droop sags, jagged clips) rather than hue; the HSV mapping is one line away if colour is ever wanted back.

Fifth iteration. Sample count raised 49 → 61 points because the summed waveform has more detail to resolve than a single sine did.

## [2026-08-16] fix | Just a line; activity states; an explicit stop

Three notes from the owner, all landing on the same session.

**"Silver dots on top of the line."** Diagnosed rather than guessed: the polyline is drawn as 60 independent segments with round caps, and the wide bloom pass was **semi-transparent**. Two translucent round caps overlap at every joint, their alpha accumulates, and you get a brighter bead at each of the 60 vertices. The dots were the glow pass, not the line.

Fixed by removing the bloom entirely — the owner asked for *just a line* — and forcing `LV_OPA_COVER`. Opaque white over opaque white is white, so joints vanish. That meant the fade in/out could no longer use opacity either, so it now mixes the colour toward black: visually identical on a black background, and every pixel stays opaque so no beading appears mid-transition. Single 3 px white stroke.

**"Display what you are doing, not just emotions."** Added six activity states — `reading`, `editing`, `building`, `testing`, `flashing`, `debugging` — as points in the same circumplex, which cost one table row each and no new rendering code. `reading` uses the `SCAN` character so the swell travels along the line like an eye scanning. 32 states total.

**"Animations need to stop when you stop."** A real design flaw: durations of 90–180 s had been in use, so an abandoned session would strand an animation on a clock nobody could read. Two fixes: `state: "clear"` (also `none`/`stop`/`idle`) reverts immediately, and `CLAUDE.md` now mandates **short 15–30 s durations, refreshed while working**, so the device self-recovers within half a minute if Claude goes quiet. Verified: a 300 s `building` was cancelled on demand, `active: true, remaining_s: 298` → `active: false`.

One transient HTTP timeout during that test coincided with the revert animation's redraw; three follow-up requests returned 200 in ~150 ms, so nothing wedged.

## [2026-08-16] design | Scaled strip, lower weather, wider vibration range

Owner reviewed the proposed screens in the spec sheet artifact, asked for the degree signs back in min/max, and approved the direction.

**Weather dropped 32 px** on the resting screen (y 302 → 334). The old spacing left 26 px under the cards and 148 px of empty black above the bezel — top-heavy. The new value centres the block in the space the cards leave.

**Line mode is now one scaled strip.** The corner clock label is gone; clock and weather are five small cards in a single centred row at y 24 — same charcoal, same radius, same seam, same colon, at 46 px tall. Since **LVGL 8 cannot transform text**, "smaller" required a second compiled face: `fliqlo_small`, 38 px, glyphs `0-9 ° / %`, ~5 KB of bitmap. Card widths are measured from the label after layout rather than using `LV_SIZE_CONTENT`, whose content measurement would feed back into a full-width seam child and fight itself.

**Vibration range widened** at the owner's request so processes are distinguishable at a glance: amplitude is now `5 + arousal^1.25 * 60`, which pushes the low end down and the high end up — `waiting` ~11 px, `reading` gentle, `building` ~40 px, `flashing` ~61 px. Activity states were re-spread across the arousal axis to use that range.

**Standing instruction recorded.** The owner pointed out the screen was often idle while work was happening, which defeats the purpose of the device. Two causes: pushes can only occur at tool-call boundaries, and — the real one — no discipline about refreshing. Now written to **cross-conversation memory** as well as `CLAUDE.md`: push at the start of every turn, refresh at each major step, 15–30 s durations, `clear` when done, and never block work if the device is unreachable.

## [2026-08-16] release | v1.0.0 — versioning introduced

Owner asked that the software be versioned on every release.

`include/version.h` is now the single source of truth, semantic: MAJOR for a breaking change to the HTTP/BLE API or the settings schema (something an existing client or a provisioned device would notice), MINOR for a new capability, PATCH for fixes and tuning.

The version is reported in **three** places — boot log, `GET /health`, and Settings ▸ Info — and each carries a compiler `__DATE__ __TIME__` stamp alongside it. That pairing matters: a version number alone cannot tell you whether the device is running the tree you are looking at, but a build stamp makes a stale flash obvious even when the number has not moved. The rule recorded in `CLAUDE.md` is to confirm the version **on the device**, never from the tree.

Release procedure, four steps in order: bump `version.h`, append to `docs/RELEASES.md`, commit and tag `vX.Y.Z`, flash and confirm via `/health`.

Tagged **v1.0.0** and verified on hardware: `version: 1.0.0 | build: Aug 16 2026 12:10:35`.

## [2026-08-16] release | v1.1.0 — weather as cards

Owner reviewed the two proposals on hardware and picked **B**: the resting weather becomes cards rather than plain text.

That is the stronger reading of "everything retains its design language" — with it, every value on the screen wears the same charcoal card with a centre seam, and hierarchy is carried by **scale and colour** rather than by two different treatments sharing one screen.

Needed a third face. `fliqlo_mid` at 44 px joins `fliqlo_small` at 38 px, because the mockup draws the resting weather larger than the line-mode strip — and it should be, so the strip stays visibly subordinate when the line takes over. `mini_card()` is now scale-parameterised and shared by both rows, as is the width-measuring sync.

Pleasing side effect: **flash usage fell 92 KB** even after adding two faces. Removing the last reference to `lv_font_montserrat_48` let the linker discard it, and a 48 px full-Latin face costs far more than two 13-glyph numeric subsets.

Followed the release procedure recorded yesterday: bumped `version.h` to 1.1.0, logged it in `docs/RELEASES.md`, committed, tagged, flashed, confirmed on the device via `/health`.

## [2026-08-16] release | v1.2.0 — Save and Close split, with an unsaved-changes guard

Owner asked for Save and Close as separate buttons, with a confirmation when closing with unsaved changes.

The request matters more than it first appears. **The brightness sliders preview live** — so under the old single "Save & close" button, a user who dragged a slider and then left had already changed the device, permanently, with no way to undo it. There was no "cancel" at all.

Splitting the actions required knowing whether anything was unsaved, which required a **snapshot** taken when the screen opens, and comparing every widget against it rather than against live settings.

That exposed a prerequisite: `slider_cb` was writing straight into `settings_get()`. Discard restores the snapshot, and a slider that had already overwritten the live struct leaves nothing to restore. Sliders now **preview only**; the committed value is read from the widget in `apply_widgets()`.

The explicit apply-now actions — Connect, city pick, Apply & sync NTP, Reset to defaults — each take a fresh snapshot, so a deliberate apply never reads as unsaved work and the dialog does not fire spuriously.

Behaviour now: **Save** commits and stays open with a green "Saved" for ~2 s; **Close** with edits pending raises *"Close without saving?"* → Discard / Keep editing; Discard restores the snapshot and undoes the live effects without touching NVS, since NVS already holds those values. The Save button turns blue whenever there is something unsaved, so the dialog is an expected consequence rather than a surprise.

Detail worth keeping: the confirmation uses `lv_msgbox_close_async()`. Deleting an object from inside its own event callback is a foot-gun in LVGL.

## [2026-08-16] release | v1.3.0 — the clock actually scales

Owner clarified what "animate" meant: the clock should **scale** into the corner and the weather should **move** to the other one. What existed was a cross-fade between two separate clocks — a different thing, and a fair correction.

**LVGL 8 cannot transform text.** `transform_zoom` applies to images; a label is drawn from a compiled bitmap face at one fixed size.

`lv_snapshot_take()` is the obvious answer and ships with LVGL, but the library sets `LV_USE_SNAPSHOT 0`. Adding a project `include/lv_conf.h` does not fix it — inspecting the real compile command shows LVGL's own sources get only `-I…/lvgl` and `-I…/lvgl/src`, so `lv_snapshot.c` can never see a project config. A `#error` probe confirmed the project copy reaches project translation units only. `LV_CONF_PATH` would work but is macro-stringified and this project's path contains a space. **Deleted the project `lv_conf.h`** rather than leave a file in the tree that looks authoritative and does nothing — that is worse than not having one, because it invites edits that silently fail.

The route that works needs no config change: **`lv_canvas` derives from `lv_img`**, so `lv_img_set_zoom()` applies to it. The big clock is drawn into a 572×232 canvas — rects, 210 px digits, seams — and that *image* is zoomed and flown. 265 KB via `ps_malloc`; measured on device as a 268 KB drop in free PSRAM.

The zoom lands on **real cards**, not a permanently downscaled bitmap, because a 4× downsample of a 210 px face looks soft and the clock stays on screen. For the swap to be invisible the landing geometry must equal the flight geometry, so everything derives from one number — zoom 61/256 = 0.2381, chosen so 210 px digits land at exactly 50 px, hence `fliqlo_corner` at 50 px. Card 64×55, radius 6, gap 9, and the digit padding works out to **4.1 px both ways**. That last match is the one that matters: the glyphs sit in the same place before and after.

Weather just moves, per the request — same 44 px cards, right-aligned by a **measured** shift since the row's width changes with its values. If PSRAM cannot supply the canvas the code falls back to the old cross-fade; a plainer transition beats none.

## [2026-08-16] release | v1.4.0 — the transition is sequential

Owner: entering, the scale must finish **before** the line appears; leaving, the line must go **before** the clock scales back.

They were overlapping — the line faded in while the clock was still flying. That reads as two unrelated things happening at once. In sequence it reads as a single movement: the clock gets out of the way, and the line takes the space it vacated.

Entering is driven by a one-shot timer that fires at the end of the flight and only then builds the line and its caption. Leaving is driven by the fade-out itself: when the line's gain reaches zero, the wave timer triggers the fly-back. Putting the hand-off inside the thing that finishes first means the order cannot slip, rather than relying on two independent durations happening to line up.

Four cases that had to behave, and now do:

- **State change while the line is up** does *not* re-run the transition. Activities change every few seconds; re-flying the clock each time would be unwatchable. Only the caption swaps.
- **New emotion during a fade-out** brings the line straight back with no spurious scale, because a `layout_small` flag records that the clock is already parked.
- **Clear mid-flight**, before the line ever appeared, simply flies back.
- **Deleting the wave timer from inside its own callback** is what the teardown does; LVGL supports this by flagging the timer and skipping its post-callback bookkeeping.

## [2026-08-16] release | v1.4.1 — seam removed

Owner, from hardware: keep the flip animation on all the numbers, but remove the centre line — it looks choppy, there are not enough pixels to play with.

The seam was in the original brief ("two large rounded dark-charcoal cards with a horizontal center seam") and had been there since Stage 1. It worked at 232 px. What changed is that the same card language now also runs at **55 px** (corner clock) and **58 px** (weather): a 2 px black line across a 55 px card has too few pixels to sit cleanly, and reads as a rendering artefact rather than as the gap between two physical flaps.

**The fold animation is untouched** — it still hinges at `CARD_H/2`. The seam was only ever the drawn hint of where that hinge is, and the fold itself demonstrates it far better than a static line did. Removing it costs nothing that the animation was not already saying.

Removed alongside it: the per-card `accent` object, a transparent child on every card that has been dead since the v4 line renderer stopped pulsing it. Also dropped `SEAM_H`/`MINI_SEAM` and the z-order calls that existed only to keep the seam above the flaps.

This supersedes a line of the original brief, so the vault was corrected rather than left contradicting the firmware — [[D012 - Card geometry]] and [[Stage 1 - Static digits]] now record the removal and why.

## [2026-08-16] release | v1.4.2 — seam back on the big clock, thinner

Owner, immediately after v1.4.1: keep the line on the time, it is big enough — and keep it thin.

The v1.4.1 removal went one card too far. The right rule is **scale-dependent, not all-or-nothing**: at 232 px a 2 px seam is a hairline that sits cleanly and says split-flap; on a 55 px corner card or a 58 px weather card there are simply too few pixels for a line to land on, which is what looked choppy. Same element, opposite verdict at different sizes.

So: big clock keeps it at **2 px** (thinner than the original 3), small cards have none.

The zoom canvas carries the seam, since it renders the big clock — it scales with everything else and is sub-pixel by the time the seamless corner cards take over, so the handover stays invisible. Restored the two `lv_obj_move_foreground(c.seam)` calls so the seam stays above the moving flaps during a fold.

Vault corrected again: [[D012 - Card geometry]] now records the scale-dependent rule rather than either absolute, and the Stage 1 note matches. Two corrections in two releases is worth noting — the underlying lesson is that a design element evaluated at one size cannot be assumed to hold at another.

## [2026-08-16] release | v1.4.3 — seam 3 px

Owner: make it 3 px. Reverted the v1.4.2 thinning; the scale-dependent rule is unchanged — big clock has a seam, small cards do not.

## [2026-08-16] release | v1.5.0 — line mode revised

Reviewed what line mode actually displays and found one defect among two matters of taste. Owner picked the fullest option.

**The defect: the corner clock had no colon.** The big clock has one, so the "true scale" celebrated in v1.3.0 was not faithful — and worse, `zoom_render()` never drew one either, so the colon *vanished the instant the flight began* and did not return until the clock did. Fixed in both places, at the same scaled coordinates, so it now shrinks with everything else.

**Temperature only while the line runs.** Min/max and humidity are reference figures — you consult them deliberately, on the resting screen. While the line is up you are watching the line. 377 px of weather becomes 86 px and the two corners stop competing. `weather_detail(false)` runs *before* the shift is measured, because the right-align target depends on how many cards are showing.

**Corners dim to 60 %.** On a true-black AMOLED this costs almost no light and reads as depth rather than as something switched off. The subtle part: the zoom canvas fades to the same 60 % on the way in, so when it hands over to the corner cards there is no brightness pop — the same discipline that made the geometric hand-off invisible in v1.3.0.

## [2026-08-16] release | v1.6.0 — the caption

Owner: the text under the line should be 60 % brightness like the clock and temperature, and should show more so the context is understandable.

The brightness half is straightforward — pure white at `DIM_OPA` rather than a baked grey, so it is literally 60 % of the line rather than an approximation of it. The line stays the only thing at full brightness.

The context half needed a decision I had been avoiding. Asked earlier what the caption showed, the honest answer was **nothing systematic**: filenames (`ui.cpp`), concepts (`lv_snapshot`), restatements of the state (`reading`), version strings, and outright jokes (`can you see me`). Four registers, no rule.

The structural problem underneath: the caption is the only text on screen, but it carried the **object** while the **verb** lived in the line's motion — which only helps if you can read amplitude. `ui.cpp` never said whether it was being read, edited or debugged.

So the caption now renders **`state` + `message`**, the message limit goes 20 → 48, and it wraps to two lines at 560 px. A state sent with no message shows the state name, so the bottom of the screen is never blank while something is happening.

Convention recorded in `CLAUDE.md` so the rule survives this session: **`message` is an object, never a verb.** Sending `message: "reading"` now renders "reading reading", which makes the mistake self-evident.

## [2026-08-16] release | v1.7.0 — weather icon and a size hierarchy

Owner: temperature and humidity should be a bit bigger than the min/max forecast, and a sun/cloud/rain glyph should sit left of the temperature.

**The hierarchy was inverted.** All three weather cards used the same 44 px face, and because "31°/41°" is the longest string, the **min/max card was the widest thing in the row** — the least important value dominating it. Temperature and humidity are live readings and stay at 44 px; min/max is a forecast you consult deliberately and drops to 34 px on a shorter card.

**Icons: researched, not guessed.** Open-Meteo serves WMO code table 4677, and its subset is 28 distinct values. Folded to **eight**: clear, partly cloudy, overcast, fog, drizzle, rain, snow, thunderstorm. Severity is deliberately discarded — at 40 px "light drizzle" and "dense drizzle" are indistinguishable, so encoding it would be noise pretending to be information. A glance needs the kind.

Drawn with **primitives rather than bitmaps**: eight icons as images would be roughly 13 KB of flash, and drawing them means they inherit the palette, stay crisp if the size changes, and cost nothing but code. Monochrome — white sun, grey cloud, dimmer precipitation — so they sit inside the existing language rather than importing a second one.

**A bug caught by testing rather than reasoning:** `memset(&wx, 0, ...)` leaves `code == 0`, and WMO 0 is *clear sky*. The panel would have shown a confident sun before the first fetch ever succeeded. Now initialised to −1, which falls through to the neutral cloud.

Verified live: `code 3` (overcast), 40.3 °C, 37 % humidity in Abu Dhabi.

Sources: [WMO interpretation codes](https://gist.github.com/stellasphere/9490c195ed2b53c707087c8c2db4ec0c) · [WMO Code Table 4677](https://www.nodc.noaa.gov/archive/arc0021/0002199/1.1/data/0-data/HTML/WMO-CODE/WMO4677.HTM) · [Open-Meteo docs](https://open-meteo.com/en/docs)

## [2026-08-16] release | v1.7.1 — min/max at half size, and a build stamp that works

Owner: min/max should be **50 % of the temperature size**. Temperature is 44 px, so min/max is now 22 px on a 32 px card — the hierarchy is a stated ratio rather than an eyeballed one.

**More importantly, the release tooling had a hole, and it caught itself.** After flashing the v1.7.0 `net.cpp` fix, `/health` still reported `build Aug 16 2026 13:37:29` — unchanged — while the image size had moved. `FW_BUILD` is `__DATE__`/`__TIME__`, which the compiler bakes in when the translation unit containing it is compiled. Change any file *other* than `main.cpp` and the stamp goes stale while the binary genuinely changes. That is precisely the failure the stamp existed to detect, so the rule "confirm on the device" was quietly resting on something unreliable.

Fixed with `scripts/version_stamp.py`, a PlatformIO pre-action that injects the git revision as a **build flag**. A flag cannot go stale — changing it forces a rebuild — and it names the source revision exactly rather than approximately. `+dirty` marks uncommitted work, which is the normal state mid-session. `CLAUDE.md` now says to trust `git` over `build`.

Worth noting the shape of this: the diagnostic was wrong, and it was wrong in the direction of false reassurance. Only comparing it against a second signal (the image size) exposed it.

## [2026-08-16] release | v1.7.2 — min/max on the temperature's palette

Owner: min/max should be on the same palette as the current temperature. Changed from `#8A8A8A` to the same white.

It is the right call now that size carries the hierarchy. When everything was 44 px, colour was doing the ranking; at 22 px against 44 px the size difference is unambiguous on its own, so a second signal is redundant and the row reads as one palette rather than three greys.

Left open: humidity is still `#8E8E8E` and is now the only grey in the row.

## [2026-08-16] release | v1.7.3 — min/max inside the temperature frame

Misread the previous instruction. "min/max need to be on the same pallet as the current temperature" meant the same **frame** — the same card — not the same palette. v1.7.2 recoloured it white; the actual ask was structural.

min/max now lives inside the temperature card, to the right of the big number: `☁ 40° 31/41`. Back to grey `#8A8A8A` at 22 px, and dropped 8 px so it sits low against the headline figure the way a range does. Putting it in the same frame says "this is context for that number" structurally, rather than leaving size to imply it.

The card measures itself from both labels, so it shrinks when line mode drops the range and grows back when the clock returns — the same measured-not-assumed approach the right-align shift uses.

Lesson for the log: the owner's phonetic spellings have been reliable signal so far ("desecrate" → discrete, "steel" → steal), so I read "pallet" → "palette" and picked the wrong homophone. "on the same X as" was the tell — you are on a frame, not on a palette.

## [2026-08-16] process | tools/say — pushing state becomes part of the work

Owner, for the second time: the display stopped showing anything about what I was doing.

They were right both times, and the second occurrence says the previous fix was inadequate. After the first lapse I wrote the rule into `CLAUDE.md` and into cross-conversation memory — and then broke it again, because a rule that says "remember to do X as a separate step" fails exactly when a turn gets long and busy, which is precisely when the display matters most.

So the fix is structural rather than another promise. `tools/say` and `tools/hush` exist so the push is a **prefix on the command that does the work**:

```bash
tools/say building "ui.cpp" && pio run
```

That cannot be forgotten independently of the work, because it is on the same line as the work. `say` is fire-and-forget — backgrounded, 2 s cap, output discarded, always exits 0 — so it can never delay or break the command it decorates, which removes any incentive to skip it.

`CLAUDE.md` and the memory entry now lead with the helper rather than with raw curl.

## [2026-08-16] fix | v1.7.5 — the temperature was zero pixels tall

Owner: no temperature in line mode, and none on the home screen either.

`card_sync()` sizes each card with `cards[i]->h`, and for the temperature card that value was **0**, so it was drawn zero pixels tall. Humidity survived purely because its branch reads `cards[i]->h ? cards[i]->h : h` — a fallback the temperature branch, written later, did not have.

**Why `h` was zero:** `mini_card()` was supposed to record it. That assignment was written in v1.7.0 and **never landed** — the scripted edit matched on the function's closing lines, missed a blank line before the brace, and `str.replace` returns the string unchanged when the pattern is absent. Silent no-op. `c.h` kept its zero-initialised value.

It then sat dormant for four releases, because nothing read `c.h` until v1.7.3 moved min/max into the temperature frame and gave that card its own sizing branch.

Two fixes: the assignment is in, and the temperature branch now carries the same `?:` fallback so a zero can never blank a card again.

**Process note.** Diagnosed by instrumenting rather than reasoning — a one-line `Serial.printf` of the computed geometry gave `h=0` immediately, after the code had *read* correct three times. Second time today that a printf beat inspection. The other lesson is sharper: **scripted edits must assert their patterns matched.** The fix patch above verifies each replacement and aborts if one is missing, which is how this should have been done all along.

## [2026-08-16] process | flipclock made global

Owner, third time: outside this chat, the display shows nothing when they talk to me. This time the meaning was **other conversations**, not other moments — a different problem from the two before it, and a real one.

Three independent causes, all confirmed rather than assumed:

1. **No global `~/.claude/CLAUDE.md`.** Nothing instructed me outside this project.
2. **Memory is per-project.** `drive-the-flip-clock-display` lives under this directory's memory folder, so a session started anywhere else never loads it. There are eleven other project memory folders on this machine that would never see it.
3. **`tools/say` is a relative path.** Confirmed: `no such file or directory` from any other cwd.

Fixed by installing `~/.local/bin/flipclock` (already on PATH) and writing `~/.claude/CLAUDE.md`, which loads in every session regardless of directory.

**Two bugs found while building it, both hidden by the same design choice.** The helper was fire-and-forget — backgrounded, output discarded, always exits 0 — which is right for something that must never break the command it prefixes, but it also meant every failure was silent.

- **Host order.** `flipclock.local` takes the *full* 2 s timeout to fail from a cold shell; the LAN IP answers in **45 ms**. Trying mDNS first cost 2 s a call. IP first now, mDNS as the fallback for when the address changes, plus a cached-host file.
- **Backgrounding killed the request.** The script backgrounded curl and then exited immediately, so the subshell died before the retry ran. At 45 ms the synchronous call is cheaper than the bug the backgrounding was hiding, so it is synchronous now.

The lesson generalises: *fire-and-forget* and *silent* are not the same property, and conflating them cost three rounds of the owner telling me the display was dark.

## [2026-08-16] process | The Chat app could never have worked — MCP bridge added

Owner, a third time and now unambiguous: the display does nothing when they talk to me **in other conversations**. I had read that twice as "other Claude Code sessions" and fixed real but different problems. It meant the **Chat** side of Claude Desktop.

There it was never a configuration gap — it was impossible. Chat has no shell, and the clock is at `192.168.0.181`, an RFC1918 address Anthropic's servers cannot route to. No amount of instruction fixes a missing network path. `claude_desktop_config.json` confirmed it: `mcpServers: NONE`, so there was no local tool bridging the gap either.

MCP is the one mechanism that closes it, because Claude Desktop runs MCP servers as **local processes** — which can reach the LAN. Wrote `~/.local/share/flipclock-mcp/server.py`: JSON-RPC over stdio, **stdlib only**, no pip, no venv, nothing to break on a python upgrade. Exposes `flipclock_say` and `flipclock_status`, with the "message is an object, never a verb" rule written into the tool description so it travels with the tool rather than depending on a doc being loaded.

Tested the whole handshake over stdio before registering it — `initialize`, `tools/list`, a real `tools/call` that lit the panel, and `flipclock_status` reading back `active: True`. Registered under `mcpServers` with the existing config backed up first and all other keys preserved.

**The pattern worth remembering:** three rounds were spent fixing progressively better versions of the wrong problem, because I inferred what "outside this conversation" meant instead of establishing it. The evidence that settled it — no shell in Chat, RFC1918 address, empty `mcpServers` — took one command to gather and should have been the first thing I did.

## [2026-08-16] process | Duration defaults — correcting an over-correction

Owner: why does the line stay on so long after the work is finished?

Because two turns earlier they had said the display showed nothing, and I fixed that by removing `hush` and raising durations to 90–180 s. That solved the first complaint and created this one. Classic over-correction: I moved the dial rather than fixing the mechanism.

The mechanism was wrong in both directions. **A duration is a guess about the future, made at the start.** Work that takes 8 s under a 150 s request leaves the line up for 142 s saying nothing; work that overruns a 20 s request goes dark mid-task.

The right model was already there and I had stopped using it: because **every work command is prefixed**, each command *refreshes* the timer. So the duration only has to outlive the **gap between commands**, not the whole task — and what the owner actually sees idle is the tail after the last push. Default is now **30 s** in the shell command, the MCP tool, and all three sets of docs, with a longer value passed explicitly for a single long-running command like a build.

Both failure modes came from treating the duration as "how long should this be visible" instead of "how long until the next refresh".

## [2026-08-16] fix | v1.7.6 — silencing the charge LED (built, not yet flashed)

Owner: make the red LED on the back stop blinking.

It is the SY6970 charge-status LED, and [[D007 - Skip SD, silence the charge LED]] deliberately left it enabled on the reasoning that it was a useful "it has power" indicator. That reasoning was wrong about what the LED means. **It reports a fault, not power.** There is no battery, so the charger can never complete a cycle, and blinking red is exactly how the part signals that. The signal is correct and permanently useless — and on an object whose entire aesthetic is a black panel with pixels genuinely off, a red light flashing forever on the back is the only thing that looks broken.

D007 said at the time it would be "trivially reversible if it turns out to be visually distracting". It did, and it was: one boolean.

**Could not flash.** `/dev/cu.usbmodem2101` has disappeared and `ioreg` reports zero USB JTAG devices, so `pio run -t upload` failed. The device is fine — `/health` answers with uptime 1057 s and RSSI −53, so it never lost power; only the USB *data* link dropped. Committed unflashed and deliberately **not tagged**: the release procedure ends with "flash and confirm on the device", and tagging something unverified would hollow that out.

## [2026-08-16] feature | v1.8.0 — a moon after dark (built, not flashed)

Owner: the weather icon should be a moon at night, worked out from the sun being up or down.

Open-Meteo has exactly the right field — **`is_day`** in the current block, which it derives from sunrise and sunset for the supplied coordinates. Using it means the device never does solar geometry, never reasons about the timezone, and stays correct automatically when the city changes in Settings. Confirmed against the same response before writing any code: sunrise 05:57, sunset 18:56, `is_day: 0` — it is night in Abu Dhabi right now, so this is testable the moment it flashes.

Only the clear-sky icons change. A cloud looks the same after dark, and every weather UI worth copying leaves the overcast and precipitation icons alone.

The crescent is carved by drawing the **card colour** back over an offset disc — cheaper and crisper than arc maths, but it only works because the icon sits inside a card of known colour. Noted in the code, because moving the icon onto the bare background would silently break it.

Still cannot flash: `/dev/cu.usbmodem2101` remains absent. Built clean, committed, deliberately untagged.

## [2026-08-16] release | v1.8.0 flashed and confirmed

USB came back — on a **different port**. `/dev/cu.usbmodem2101` had become `/dev/cu.usbmodem1101`, because macOS names the port after the physical socket and the cable had moved. That is the whole explanation for the earlier `Error 2`: nothing was broken, the address had changed.

Flashed by detecting the port from the USB ID `0x303A:0x1001` rather than trusting the name, and `CLAUDE.md` now documents that instead of the hardcoded path. The device identity is stable; the port name is not.

Confirmed on hardware: `version 1.8.0 | git 1ed02e0` — **no `+dirty`**, so what is running is exactly the committed tree, which is precisely what the git stamp was added for two releases ago. `weather_code 1`, `is_day false` → the moon is showing, at 35.3 °C and 68 % humidity.

Both queued changes are now live and tagged: the crescent after dark, and the charge LED silenced.

## [2026-08-16] release | v1.9.0 — flip 180°

Owner: flip the screen 180 degrees.

Rotation **2** is the other landscape — identical 600×450, and `setRotation()` re-applies the CST226SE swap and mirror alongside the MADCTL write, so touch follows the screen without any work on our side. Because the dimensions do not change, LVGL needs no re-init, which is what makes this a **live toggle** rather than a reboot: `Settings ▸ Screen ▸ Flip 180`, applied the instant the switch moves, since you cannot judge an orientation you cannot see. Persisted to NVS; defaults on, as that is how the clock actually sits.

[[D004 - Rotation 0 is already landscape]] said never to call `setRotation()`, so it is now marked superseded in part — but the finding underneath it is exactly why this was one line. Landscape is native, so flipping is a choice between two landscape rotations rather than a re-layout.

**Boot-looped on the first flash:** `Guru Meditation Error … LoadProhibited`, `EXCVADDR 0x00000008`. `app_apply_rotation()` repaints through `lv_scr_act()`, and the boot call runs **before** `beginLvglHelper()` — so LVGL was uninitialised and `lv_scr_act()` returned NULL. The repaint is now guarded on `lv_disp_get_default()`; only the Settings toggle needs it, because nothing has been drawn at boot.

Worth noting how cheap that diagnosis was compared with the ones earlier today: the register dump named the fault type and the exact null offset, so it took one read rather than any instrumenting.

## [2026-08-17] release | v1.10.0 — brightness follows the sun, or the clock

Owner reported two things: settings reverting to defaults after a power cycle, and the screen sitting at night brightness in daylight.

**The second was real and precisely diagnosable.** Local time 06:10, sunrise 05:58, `is_day: 1` — but the configured night window runs 21:00–07:00. The schedule was doing exactly what it was told and was simply wrong about the world. A fixed window is wrong every morning between sunrise and the window's end, and drifts across the year.

Fixed by preferring `is_day`, which Open-Meteo already supplies for the weather icon, computed from sunrise/sunset at our coordinates. It costs nothing extra, tracks the seasons, and follows the city if that changes. The hour window stays as the fallback when weather has never arrived, so a device with no network still dims sensibly — and the owner asked for the choice to be explicit, so it is a switch: `Settings ▸ Screen ▸ Dim by sunrise`, default on.

**The first did not reproduce.** After a hardware reset the device restored `day=255 night=65` from NVS — plainly not the defaults of 90/25. Persistence is working. The likely explanation is the Save/Close split added in v1.2.0: the brightness sliders preview **live**, so a change is visibly applied, and pressing **Close** then discards it. That is a trap of my own making — a control whose effect you can see is one you assume is committed. `/health` now reports the persisted brightness so this can be checked without opening Settings, and the question is back with the owner rather than guessed at.

## [2026-08-17] fix | v1.10.1 — "weather 496370 h ago"

Owner tapped the screen and got a weather age of 496370 hours. That is 56.6 years: the age of the Unix epoch, and arithmetically just `now / 3600` — which pins `last_sync` at exactly 0.

Weather and NTP both start from the same `GOT_IP` event, on adjacent lines. Weather usually wins the race, so the fetch was stamped with a near-zero epoch; when NTP subsequently stepped the clock to 2026, the subtraction measured the distance back to 1970 instead of the age of the reading.

**Wall-clock time is the wrong instrument for measuring an interval on a device whose clock can jump.** Age now comes from `millis()`, which cannot be stepped, and `uint32` subtraction wraps correctly at the ~49.7-day rollover so it survives that too.

All three call sites — the tap overlay, `/health`, and Settings ▸ Info — were each doing their own `time(nullptr) - last_sync`. They now share a single `net_weather_age_s()`, because three copies of the same arithmetic is three chances to fix it in one place and leave it broken in the others.

Verified across a boot: `-1` (never) → `7 s` → `20 s`.

## [2026-08-17] docs | Vault brought current before the app platform

Owner asked for a timer reached from an app drawer, and — explicitly — for the project to be documented **before** any of it was built. That was the right call: the substance of the request is *"restructure the code so you can add apps to it"*, and a timer bolted onto `ui.cpp` would have satisfied the letter of it and none of the point.

Three things were out of date or missing:

- **No module map.** Twelve source files with no statement of what each owns. Written now, and framed around what each file may *not* do — the threading rule, the two load-bearing boot orderings — because those are the constraints that actually get violated.
- **`ui.cpp` is 1333 lines** carrying five separate concerns: the clock, the gesture machine, the emotion renderer, the corner clock, the zoom canvas. That is the pressure the app platform relieves, and it is now recorded as the motivation rather than left as an unexplained refactor.
- **The index's status callout was hours stale**, still describing BLE as unverified.

Two decisions written before the code, which is the point of documenting first:

**[[D026 - Apps are a platform, not a special case]].** The pattern already exists — `ui_settings.cpp` is an app in everything but name: own screen, opened by a gesture, tears down and returns home. The restructure *names and generalises* something already working rather than inventing an abstraction, which is a far safer move. Also records what is deliberately **not** an app: the clock, because it is the resting state of the object and demoting it to a tile would contradict every other decision in this project.

**[[D027 - The gesture budget]].** The owner's constraint was "don't break others", and the hazard is specific: a swipe *begins as a press*, so a naive fourth gesture makes an upward drag also register as a tap — or worse, as a long press that silently changes brightness on the way to opening the drawer. Three rules keep them disjoint, the important one being that **displacement disqualifies a press** regardless of duration. Also notes that four gestures is close to the limit for a screen with no affordances, and that apps have no budget problem because they can afford real buttons with labels.

## [2026-08-17] release | v1.11.0 — the app platform and a timer

Built to the design written in the previous turn, which is the point of having written it: the contract, the drawer, the timer, and the gesture all landed without re-litigating anything.

`app_api.h` is five fields. `app_host.cpp` owns the registry, the drawer, launching, and the route home — and nothing else; it never learns what an app draws, and no app learns anything about the clock. Adding an app is now **one file in `src/apps/` and one line in the registry**.

The timer reuses the clock's cards and 210 px digits deliberately: a second visual language on one device would be one too many. Drag sensitivity is deliberately non-linear — whole minutes above ten, 15-second steps below — so a 45-second egg and a 40-minute bake are each a single gesture rather than one being an exercise in patience.

**The gesture worked out exactly as D027 predicted it would need to.** Displacement disqualifies a press regardless of duration; without that an upward drag also fires a tap, or a long press that silently changes brightness on the way to the drawer. The swipe must start within 80 px of the bottom, and the hold indicator is suppressed the moment movement begins.

One genuine C++ trap on the way: `const App app_timer = {...}` has **internal linkage**. A const object at namespace scope is TU-local by default in C++, so the registry's `extern` declaration found nothing and the link failed. `extern` on the *definition* is required. Worth remembering, because the error message names the symbol without hinting at linkage.

Not yet verified: every touch path. The device cannot be driven from here, so swipe, tap, long-press, hold, and the timer controls are all with the owner.

## 2026-08-17 — Settings as an app, and rollers that flip (v1.12.0)

Three changes from one message, and the third one only looked contradictory.

**Settings joined the drawer.** `apps/app_settings.cpp` is a thin adapter over
`ui_settings_create`/`ui_settings_destroy` — the settings screen itself is
untouched. `ui_settings_open()` now routes through `app_host_launch()`, so the
3-second hold and the drawer tile land in exactly the same place. That gesture
stays: it is a year of muscle memory and there was nothing to gain by spending
it.

**Apps are reorderable.** Long-press a tile and it moves one place left,
wrapping at the front. `app_order[8]` in NVS, so it survives a power cycle and
a re-flash like everything else.

**The timer's set gesture was wrong and is now right.** It had one drag target —
the whole face — which could only ever change one number, and gave no hint
which. The ask was "the layout like *Night from*, but for every number", keeping
the current design, with the digits animating like a flip clock. Read once that
sounds like three incompatible things; *Night from* is an `lv_roller` and a
roller looks nothing like a flip card. But *Night from* is not a roller because
rollers are good — it is a roller because it gives **one control per number**.
That is the part being asked for. So each card became its own roller, and kept
its face. [[D028 - Set a number by dragging the number]].

Three details worth keeping:

- The value is recomputed from **total displacement since touch-down**, not
  accumulated per event. Accumulating drifts, and makes the gesture
  irreversible — drag back to undo and you land somewhere else.
- A card already mid-fold **takes the new value without queueing another
  animation**. Dragging outruns a 220 ms fold easily and stacked folds tear.
- The card keeps `LV_OBJ_FLAG_CLICKABLE`. It is the control, so it is the one
  object on that screen that must not go through `decor()`.

The fold is a second copy of the clock's, not a shared helper — extracting the
original would mean refactoring the screen that has worked all day in order to
add a feature elsewhere. Trigger written down: if a third caller appears,
extract it then.

**Cost of the patch-don't-rewrite instinct.** Splicing the rollers into
`app_timer.cpp` put the new `Digits`/`flip_to` block *after* `render()`, which
calls it — five errors, all ordering, plus a duplicate `dragging` and a dangling
`face_cb`. Rewriting the file whole took less time than the second patch would
have. When a change moves declarations, patching is the slower option.

RAM 18.3 % · Flash 26.5 %.

## 2026-08-17 — The way back (v1.13.0)

Asked for the phone gesture: swipe in from the left edge to go home.

The obvious build is an event handler on the app's screen, the way the clock
handles its own gestures. It does not work, and the reason is worth writing
down: **LVGL 8 does not bubble events**. Any clickable child eats the press
first — the timer's minutes card (which is a roller and must stay clickable),
the settings tab bar, its lists, its rollers, every button on both. The gesture
would have worked on empty background and failed on precisely the widgets you
are most likely to be touching. Setting `EVENT_BUBBLE` on every object an app
ever creates is [[D014 - Touch hit-testing]] again with a new way to forget.

So it is **polled from `app_host_tick()`**, above the widget tree. That is where
a system gesture belongs; apps get it for free and none of them can break it.
Same shape as the clock's swipe-up — armed by where it starts, judged on
release — because two gestures that behave differently read as two systems.
[[D029 - Back is a system gesture, not a widget event]].

Two things that would have been silent bugs:

- **Judge the last point seen while pressed**, not the point read after release.
  The latter trusts the touch driver to leave valid coordinates behind, and a
  driver that zeroes them would make the gesture impossible to perform with
  nothing in the log to say why.
- **The timer's card overlaps the edge zone** (x = 14…282), and `set_seconds` is
  a static that outlives the screen — so swiping home across a card would have
  left the timer set to a number nobody chose, discovered next time you opened
  it. The roller now abandons a drag that turns sideways and puts the value
  back, which is a better roller regardless.

`App` gained one optional field, `back()`, called before routing home. It exists
for a specific thing: Settings builds its text editor as a **child of its own
screen**, so `lv_scr_act()` cannot tell the host that typing is in progress, and
a stray edge swipe would have discarded a hand-typed Wi-Fi password. It closes
the editor and keeps the text — closing commits nothing to NVS, so keeping costs
nothing and losing costs a password.

**Vault housekeeping, found while auditing.** `.pio/` contains 48 markdown files
against 44 real notes, and `.obsidian/` had no ignore filter — so more than half
of this vault's graph, search results and wikilink autocomplete was vendored
library READMEs. Added `.obsidian/app.json` with `userIgnoreFilters`. Folded
that and the rest of this project's vault conventions back into the
`obsidian-markdown` skill as two new references, `VAULT-IN-A-REPO.md` and
`DECISION-RECORDS.md`.

RAM 18.3 % · Flash 26.5 %.

## 2026-08-18 — Retire a gesture, fix a reboot, make the timer usable (v1.14.0)

**The 3-second hold is gone.** It was built when Settings was the only other
screen and there was no other way in; Settings is an app now and the drawer goes
there. D027 said in as many words that every gesture makes the next one harder —
the honest first one to spend is the one whose reason expired.
[[D030 - Retire the 3-second hold]]. The press path got simpler as a side
effect: nothing happens *during* a press any more, so `settings_fired`, the
accent bar and the whole progress branch went with it.

**The drawer reorder reboot.** Long-press a tile, device resets, every time. The
handler deleted the drawer and rebuilt it — from inside an event callback on one
of that screen's own grandchildren. Deleting the active screen leaves
`lv_disp_t.act_scr` dangling and the first `lv_obj_create()` of the rebuild
walks up to invalidate through it. Not a race; structural.

Nothing needed deleting. The tiles are flex children and a flex container lays
out in child order, so `lv_obj_move_to_index()` **is** the reorder. The layout is
the model. [[D031 - The layout is the model]].

Reading that code turned up a second bug sitting behind the first: **LVGL sends
`LV_EVENT_CLICKED` on every release, including the end of a long press.** So the
reorder was always going to be followed by launching the app it had just moved —
it just never got that far, because the reset came first. `SHORT_CLICKED` is the
one that fires only for short presses. The timer's cards had the same trap from
the other direction and now use it.

**The timer was "tricky", and the reason is arithmetic.** At 13 px a step on a
450 px screen a full-height drag is 34 steps, so 45 seconds took two drags and
an exact landing took a third — with a 220 ms fold running behind the finger, so
the number you were aiming at was never the number on screen. Making the drag
faster fixes the range and makes the aim worse. Opposite problems; one control
cannot solve both.

So: three gestures on the same card. Drag coarse at 18 px a step (*calmer*, now
that it is not the only way), tap above or below the middle for exactly ±1, hold
for ±1 at ~10/s — full range of seconds in about six seconds. Fold down to 70 ms
a phase, because the clock folds once a minute and the fold is the point, while
here it fires on every step. Seconds wrap, minutes clamp.
[[D032 - Three gestures, one control]].

Also: a split seam across each card so the timer reads as the same object as the
clock; Start and Reset became full-width 122 px cards under the numbers they act
on (no seam — that line means *this flips*, and through a word it is a
strikethrough); no coloured frame around a running number, because a border
around a number reads as an error box and the button already says Pause.

**The Clock button is gone at the owner's request, so the left-edge swipe is now
the only way out of the timer.** That is worth stating plainly: if the gesture
does not work on hardware, the timer is a trap and the way out is a power cycle.
A thin dim bar sits at the left edge so the gesture has an affordance at all.

RAM 18.3 % · Flash 27.2 %.

## 2026-08-21 — Back means back (v1.15.0)

The left-edge swipe went straight to the clock from anywhere. With one app that
was indistinguishable from "back"; with a drawer in the middle it stopped
matching the path walked — leaving the timer threw you past the drawer, and
getting to Settings from there was two more gestures. The owner asked for the
stack model, and for visible back buttons — v1.14.0 had left the timer with an
invisible gesture as its only exit, which the log called out as a trap at the
time.

`app_host_back()` now owns what back means: app → drawer → clock, one level per
invocation. The swipe, the timer's new back card, Settings' bottom-bar button
(relabelled from Close) and the drawer's Clock button all route through it —
one door, so the next change to back's meaning happens in one place. The
app-first-refusal hook survives unchanged inside it, so Settings with the
editor open still closes the editor and stays put.

Two details:

- **The drawer is rebuilt on every visit.** Backing out of an app calls the
  same `build_drawer()` that swipe-up uses. Keeping a drawer resident would
  have been the tempting shortcut — and a stale copy the first time the app
  order changed underneath it.
- **Timer controls became three cards** — back · Start · Reset at 120/202/202.
  Start and Reset gave back 66 px each of v1.14.0's width; still 2.6× their
  pre-1.14 area.

D033 written; D029 annotated — its destination half is superseded, its
architecture half (polled above the widget tree, first refusal) stands.

RAM 18.3 % · Flash 27.2 %.

## 2026-08-22 — Updates ship over the air (v1.16.0 / v1.16.1)

The owner asked for OTA as step one of the "real OS" direction. The plan had
budgeted a v2.0.0 for this — new partition table, NVS wiped, everything
re-provisioned. Checking before believing paid off in the right direction for
once: `default_16MB.csv`, in use since Stage 0, already carries `ota_0` +
`ota_1` + `otadata`. The second slot sat there unused for the project's whole
life. No repartition, nothing wiped, MINOR release.

`POST /update` on the existing async server, raw body (curl is the client, so
the size is known up front and the magic byte kills wrong files in chunk one),
reboot hooked to the client's disconnect so the 200 demonstrably lands before
the restart. The upload posts `flashing` through the emotion queue — the
device shows its own update the way it shows everything else. `tools/ota`
builds, pushes, and refuses success until `/health` reports the new version,
which is the release checklist's trust-the-device rule turned into code.

Written down as deliberately absent: auth (private LAN; token hook noted),
rollback (stock Arduino core cannot — **the cable stays the rescue path**),
resume. And v1.16.0 itself crossed the cable one last time, since the running
firmware had no endpoint to receive it — v1.16.1 is a version-bump-only patch
whose entire purpose is to be the first release delivered by the mechanism it
proves.

RAM 18.3 % · Flash 27.3 %.

## 2026-08-22 — The Lab (v1.17.0)

Third app: the clock as a bench tool — GPIO control, I2C scanner, UART
monitor. The feature work was ordinary; the design work was one question:
**which pins may an app touch on a board where most pins are spoken for?**
Answered by cross-referencing the schematic's header netlist against the
library board config, and the answer had a trap in it: GPIO18 reaches the
header AND is the display's tearing-effect line. On the header ≠ free —
that pin is the reason the whitelist is written down as a decision (D035)
rather than living silently in an array.

Internal I2C (6/7, PMU + touch + the P4 plug) is scannable but never
drivable; the external scanner runs on Wire1 so the touch driver's bus is
never reconfigured under it. UART is Serial1 on any whitelist pair, 43/44 by
default because that is what they are. destroy() returns every pin to Hi-Z
and releases both peripherals — launching and leaving the Lab is
electrically invisible.

One self-caught bug worth keeping: the UART monitor replaced unprintable
bytes with a bare 0xB7 ('·'). LVGL labels are UTF-8; a lone high byte is not
valid UTF-8 and corrupts the decode of everything after it. Plain '.' now.
Same genre as the D025 lesson — the display stack's encoding assumptions are
load-bearing.

First feature shipped entirely over the air: tools/ota, no cable touched.

RAM 18.7 % · Flash 27.7 %.
