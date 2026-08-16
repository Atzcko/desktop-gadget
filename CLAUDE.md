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

## Versioning — every release

`include/version.h` is the single source of truth. **Semantic versioning:**

| Bump | When |
|---|---|
| **MAJOR** | breaking change to the HTTP/BLE API or the settings schema — something an existing client or a provisioned device would notice |
| **MINOR** | a new capability: a transport, a screen, a feature |
| **PATCH** | fixes and tuning that change nothing anyone integrates against |

**Every release does all four, in this order:**

1. bump `FW_VERSION_*` in `include/version.h`
2. append the release to `docs/RELEASES.md` with what changed and why
3. commit, then tag: `git tag -a v1.2.3 -m "summary"`
4. flash, and confirm the running version with `GET /health`

> [!warning] Confirm the version on the DEVICE, not in the tree
> Reported in the boot log, `GET /health`, and Settings ▸ Info. A tree that says
> 1.1.0 proves nothing about what is running.
>
> **Trust `git`, not `build`.** `FW_BUILD` is `__DATE__`/`__TIME__`, baked in
> when *main.cpp* compiles — edit any other file and it goes stale while the
> binary changes. `FW_GIT` is injected as a build flag by
> `scripts/version_stamp.py`, so it cannot: a changed flag forces a rebuild.
> `+dirty` means uncommitted changes are in the image.

## Hard constraints

- **LVGL 8.4.0, pinned.** Not 9.x. `LV_Helper.cpp` is `#if LVGL_VERSION_MAJOR == 8`; upgrading breaks the link, not just the API. ([[D002 - Pin LVGL to 8.4.0]])
- **Do not hand-roll board settings.** The env is copied verbatim from the library. Every flag is load-bearing. ([[D003 - Copy the library platformio env verbatim]])
- **Do not use TFT_eSPI for the panel.** It has no QSPI transport. ([[D001 - Use the LilyGO AMOLED library, not TFT_eSPI]])
- **No BLE in v1.** That is the later HID phase.
- **No deep sleep, no battery logic.** USB-powered, always on.
- **`config.h` holds first-boot DEFAULTS ONLY.** Live configuration lives in NVS. Read `settings_get()`, never a `DEFAULT_*` macro, outside `settings_reset()`. ([[D013 - Settings live in NVS, config.h is only defaults]])
- **Wi-Fi credentials are never in a file.** They are typed on the device. Do not ask the owner for their Wi-Fi password. ([[D016 - Wi-Fi is provisioned on-device]])
- **Position self-aligning widgets with `lv_obj_align()`, never `lv_obj_set_pos()`.** In LVGL 8 alignment is a persistent style property, so `set_pos` on an aligned object is an *offset from the alignment point*, not a position. ([[D019 - Text entry gets its own screen]])
- **Any new `lv_obj_create()` that is decorative must call `decor()`.** Every LVGL object is CLICKABLE by default and events do not bubble; a decorative object that keeps the flag will silently break the hold gesture. ([[D014 - Touch hit-testing]])
- **Never call LVGL from a non-LVGL task.** The weather task and the async web server handlers post to a queue; only the LVGL loop touches LVGL objects.

## Using the device

**Hold a finger anywhere on the screen for 3 seconds** to open Settings. A thin accent bar fills along the bottom edge from ~1.2 s so you can see the hold registering.

| Gesture | Action |
|---|---|
| Tap | date + weather sync age, 5 s |
| Long press 1.2–3 s | cycle brightness |
| Hold 3 s | Settings |

Settings tabs: **Wi-Fi** (scan / join), **Time** (zone, 24 h), **Place** (city search), **Screen** (brightness, night hours, toggles), **Info** (diagnostics, reset).

Settings persist in NVS and **survive re-flashing**. Only "Reset to defaults" clears them.

## Emotion API

The clock can express what we are doing. Two transports, **identical JSON**.

### Standing permission — narrate what you are DOING

> [!important] Claude: you may call this endpoint unprompted
> The device's primary job is to show **what you are working on right now**.
> Emotions are secondary — use them when they genuinely communicate something.
> No need to ask first.
>
> **Activity states** (use these most of the time):
>
> | Doing | Send |
> |---|---|
> | reading code, grepping | `reading` |
> | writing or editing files | `editing` |
> | compiling | `building` |
> | running tests | `testing` |
> | uploading firmware | `flashing` |
> | chasing a fault | `debugging` |
> | web research | `searching` |
> | reasoning before acting | `thinking` |
> | blocked on the owner | `waiting` |
>
> **Emotions** when they say something an activity cannot: `success`, `error`,
> `celebrate`, `frustrated`, `confused`, `relief`, `proud`, `sleepy`. The full
> set is 32 states; `GET /health` and `emotion_from_name()` are the authority.
>
> **Write `message` as an OBJECT, never a verb** (≤ 48 chars). The caption
> renders `state` + `message`, so the state already supplies the verb:
>
> | Send | Screen shows |
> |---|---|
> | `building` + `"ui.cpp"` | **building  ui.cpp** |
> | `reading` + `"LVGL canvas docs"` | **reading  LVGL canvas docs** |
> | `debugging` + `"wifi reconnect backoff"` | **debugging  wifi reconnect backoff** |
> | `thinking` + *(none)* | **thinking** |
>
> So: never send `message: "reading"` — that renders "reading reading". No
> jokes, no status restatements. It wraps to two lines, so a real phrase fits.
>
> **The line vibrates in proportion to the process.** Amplitude, frequency and
> speed all scale with the state's arousal — `waiting` is nearly a flat line at
> ~11 px, `reading` gentle, `building` busy at ~40 px, `flashing` a full-height
> scribble at ~61 px. Pick the state that matches the real intensity of the work
> and the display reads correctly from across the room without being read.

> [!important] Use `tools/say` — do NOT push as a separate step
> ```bash
> tools/say building "ui.cpp" && pio run
> tools/say reading "LVGL canvas docs"; grep -rn lv_canvas src/
> tools/hush          # at the end of the turn
> ```
> **Prefix the command that does the work.** Pushing the state as its own
> separate call is a step that gets skipped once a turn runs long — this has
> failed twice in practice, both times noticed by the owner rather than caught
> here. Attaching it to the work makes it part of the thing being done.
>
> `say` is fire-and-forget: backgrounded, 2 s cap, output discarded, always
> exits 0. It cannot delay or break the command it prefixes. Default duration
> 60 s, which covers a build; pass a third argument for longer work.
>
> **Conversational turns count too.** A question answered in prose runs no
> commands, so there is nothing to prefix — which makes it the likeliest moment
> to leave the panel dark. Run `tools/say thinking "<topic>"` on its own before
> answering anything substantive. One extra call, and it is the whole point of
> the device.

> [!important] Keep it lit while you work
> The owner built this device to see what you are doing **from across the desk**.
> A dark screen during a long stretch of work defeats the point of the object,
> and they have called this out more than once.
>
> - Push a state at the **start of every turn**, and refresh at each major step —
>   switching from reading to editing, before a build, before a flash.
> - Pushes can only happen at **tool-call boundaries**. During pure reasoning
>   nothing can be sent, so cover the gap by setting a duration that spans the
>   expected work.
> - If the device does not answer, carry on silently. Never block work on it.

> [!warning] Two rules that keep the clock usable
> **1. Short durations, refreshed.** Use `duration_s` of 15–30 s and re-send as
> the work continues. Then if you stop, crash, or the session ends, the device
> returns to the clock on its own within half a minute. Long durations strand
> an animation on a clock nobody can read.
>
> **2. Always stop explicitly when you finish a turn:**
>
> ```bash
> curl -X POST http://flipclock.local/emotion -H "Content-Type: application/json" -d '{"state":"clear"}'
> ```
>
> `clear` / `none` / `stop` / `idle` all revert immediately — the line fades
> out, the clock slides back to centre and the weather returns. The clock is
> the default state of this object; an animation is a temporary visitor.

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

**Pairing from macOS Bluetooth settings.** With `Settings ▸ BLE ▸ Pairable` on
(the default), the device advertises a HID service — the only thing that puts it
in System Settings ▸ Bluetooth. Its identity is a **custom vendor-defined
"desktop gadget"**, not a keyboard, so Keyboard Setup Assistant does not appear
([[D022 - Custom HID identity, not a keyboard]]). If a host refuses to list it,
`Settings ▸ BLE ▸ Identify as keyboard` is the compatible fallback.

> [!warning] Hosts cache the HID descriptor per bond
> After changing that switch you MUST forget the device on the host, Clear
> pairings on the clock, and pair again — otherwise the old identity persists.

If pairing wedges, clear it on **both** ends: *Settings ▸ BLE ▸ Clear pairings*
on the device, and *Forget This Device* on the Mac.

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
