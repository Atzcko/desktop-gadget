---
title: Releases
type: index
tags:
  - release
---

# Releases

Semantic versioning; the scheme and the release procedure live in [[CLAUDE]].
The version is reported by the boot log, `GET /health` and Settings ▸ Info,
each with a compiler build stamp so a stale flash is detectable.

## v1.29.0 — 2026-08-28

**The clock is a Bluetooth trackpad.**
([[D051 - The clock is a trackpad; the mouse is real HID]])

A standard mouse (Report ID 2) joined both HID identities, so the paired Mac
sees a real pointing device — no companion. The Trackpad app: one finger
moves (1.5 gain, fractional carry), a still tap left-clicks, a TWO-finger
tap right-clicks (raw CST226 point count; LVGL only ever sees finger one),
two-finger drag scrolls, and Left/Right strip buttons click without moving.
Leaving the app always releases every button.

**Re-pair required**: hosts cache the HID descriptor per bond — forget the
device on the Mac, Clear pairings on the clock, pair again.

## v1.28.0 — 2026-08-28

**A YouTube app — the dashboard-and-remote, exactly as assessed.**
([[D050 - YouTube is a dashboard and a remote, not a player]])

Latest uploads from up to six configured channels, newest first, with real
thumbnails (fetched and ROM-TJpgDec-decoded on a worker task, ½-scaled to
160×90 rows). Tap a video and the Mac companion (`tools/ytserve`) opens it —
the clock is the remote, the Mac is the screen. Setup lives in the app (API
key, channels, companion IP — keyboard overlays, NVS, never a file) and
everything is POSTable for provisioning from a shell. Refresh costs ~12 of
the 10 000 daily quota units.

## v1.27.4 — 2026-08-28

**The tap overlay and the weather no longer print over each other in
portrait.** Both claimed BOTTOM_MID; the overlay is a 5-second visitor, so
the weather now YIELDS to it and returns when it leaves — the manners the
emotion line has always had. Portrait-only (landscape's bottom strip is
free; line mode parks weather in a corner), the owner's weather on/off
setting is respected on return, and a yield cannot leak across a rotation
rebuild.

## v1.27.3 — 2026-08-28

**The Screen page has a layout system instead of a wrap.** Four row rules
(D048 addendum): one setting per row; label left, control right; sliders get
label + live value above a full-width bar; groups titled by section headers.
Grouped as Brightness / Night / Display / Rotation. The rules are helpers
(`setting_row`, `slider_row`), orientation-proof by construction, ready for
the other pages.

## v1.27.2 — 2026-08-28

**The Screen page scrolled sideways; now nothing in Settings can.** Two
causes, one of them v1.27.1's own fix: LVGL containers are scrollable by
default, so a flex row wider than the pane scrolls HORIZONTALLY rather than
wrapping — and v1.27.1's PCT-width `body_label` made every label inside a
row claim the full row width, guaranteeing the overflow. All eight row
containers now wrap (`ROW_WRAP`), size to content, and have SCROLLABLE
cleared — a row physically cannot scroll any more, in any direction. Labels
are content-sized again, with explicit wrap only on the genuinely long ones
(Wi-Fi status, Info diagnostics).

## v1.27.1 — 2026-08-28

**Settings text no longer runs off the screen.** Eleven fixed widths from the
600-wide tabview era (560/540/520/440/400 px) were living inside D048's
410 px landscape pane, and the label helpers created unconstrained labels
that size to their text — the Wi-Fi status line and the Info diagnostics
sailed off the right edge. Every width is percentage-of-pane now and every
helper label wraps; growth is vertical only, which is the direction that
scrolls. No horizontal scrolling was added anywhere — the rule stands.

## v1.27.0 — 2026-08-28

**Themes** ([[D049 - The look is a table]]). Theme 0 is Fliqlo, untouched.
Theme 1 "Pop" is the bento-widget board: blue hour card, red minute card,
yellow colon, orange/blue weather pair, palette-cycled drawer tiles, orange
chrome, chunkier corners — on the same true black. A Themes app previews
each theme from its own color table and applies it as an in-place rebuild
(the rotation machinery, minus the rotation). `POST /theme` and a `theme`
field in `/health` keep it drivable from a shell.

## v1.26.0 — 2026-08-28

**Settings navigates like Apple's** ([[D048 - Settings navigates like Apple's]]).
The tabview is gone: landscape shows a macOS-style sidebar (vertical section
list + pane), portrait shows an iOS-style stack (full-width list, tap pushes
the page, back pops it). Everything scrolls vertically; no horizontal gesture
survives in Settings, so nothing competes with the left-edge back swipe. The
560-px lists went percentage-width — they had been silently clipping in
portrait. Content code untouched; only the plumbing changed.

## v1.25.0 — 2026-08-28

**Messages looks like a messenger now.**
([[D047 - Messages are conversations, and the clock wears the badge]])

- Chats list → thread of bubbles (theirs charcoal left, ours green right),
  composer on the keyboard; New takes a **name or an IP** typed directly.
- The contact book learns addresses from scans, from sends, and from the
  source IP of everything received — replying never needs a scan.
- **An envelope badge on the clock** (top-left, tappable → opens Messages,
  yields to line mode like the battery chip) appears when something arrives
  and clears when you read it.
- `/messages` reports direction, peer and `unread`.

*Release-discipline note:* the first push of this feature went out without
the version bump — the image changed, `FW_VERSION` did not, and only the git
hash in `/health` told the truth. `FW_GIT` existing is what made the slip
visible; this entry and the re-push are the correction.

## v1.24.2 — 2026-08-28

**Restores the Timer's portrait_ok flag** — lost when a patch script died on
an earlier assert and everything queued behind it silently never applied.
`/health` gains `panel` (the live LVGL resolution) so a silently-borrowed
landscape can never again hide from remote verification.

## v1.24.1 — 2026-08-28

**Rotation happens IN the app now.** v1.22.0 sent you to the clock on every
BOOT press, on the argument that orientation is a device-level act. The
owner's hands disagreed — you rotate the thing while using the thing, and
being thrown home reads as a crash. The live path now remembers what was
open, rotates, rebuilds the clock for the new shape, and reopens the same
app (or the drawer) — which lays itself out for the new shape, since that is
what D045 made apps do. The one casualty, stated: rotating inside Settings
discards unsaved edits, because create-on-entry apps cannot be rebuilt
mid-edit.

**blink.lua opted into portrait** — the first script to carry the
`portrait_ok` first-line tag and branch on `SCREEN_W/H`. Its selector row
fills the width, the state card drops below, and its Back/Start sit in the
uniform strip. This is also why the Timer "wasn't affected by rotation":
pressing BOOT inside it went home, so its portrait layout was never seen.

## v1.24.0 — 2026-08-28

**Every shape, one chrome, and the gadgets can talk.**

- **Native apps are orientation-native** ([[D045 - One chrome, every shape]]):
  the Timer stacks its cards in portrait, Settings and the Lab reflow (fixed
  grids became wrapping flex), and D043's borrowed-landscape now applies only
  to scripts that have not tagged `portrait_ok` in their first line. Scripts
  get `SCREEN_W`/`SCREEN_H` to branch on.
- **One back chip everywhere**: bottom-left, 132×56, "← Back" — timer, Lab,
  Settings (still guarding unsaved changes), Messages, and the drawer, whose
  button now names the gesture rather than the destination.
- **Messages** ([[D046 - Gadgets message over HTTP and mDNS]]): a new app and
  a new service. Identity is the device name; peers are one mDNS browse away
  (`_gadget-msg._tcp`); delivery is `POST /msg` into the server every device
  already runs; arrivals announce themselves on the line display and land in
  a 16-deep inbox. `GET /messages` and `POST /send` make the whole loop
  drivable from a shell. ESP-NOW and MQTT considered and rejected for cause.

RAM 19.4 % · Flash 30.8 %.

## v1.23.0 — 2026-08-26

**The battery gauge counts coulombs where the hardware can measure them.**
([[D044 - Coulombs where measurable, model where not]])

The SY6970 measures charge current but cannot see discharge current, so:
charging integrates the **measured** mA (real coulomb counting), charge-done
snaps to 100 %, and discharge integrates a brightness-aware **model** tethered
to the voltage curve (~50 min time constant) so error cannot accumulate. State
survives reboot via NVS; `batt_mah` (default 5000) scales it; `/health` gains
`gauge`, signed `ma`, and `mah_used` since last full.

Also fixes the chip showing green on a full battery: the library's
`isCharging()` counts DONE as charging (and `isChargeDone()` returns the
opposite of its name — two more of the D042 genre).

A true counter remains a ~3 € INA226 on the battery lead via the Lab's I²C
pins; `gauge_update()` is the seam where it would slot in.

## v1.22.1 — 2026-08-25

**BOOT uses the internal pull-up.** The first remote rotation test caught a
phantom press — asked 180, landed 90. The "external 10 K pull-up" the code
trusted was an unverified claim in our own T4-S3 note; the schematic shows
none, so plain `INPUT` floated. The note is corrected, not quietly edited.

## v1.22.0 — 2026-08-25

**Any orientation, and BOOT cycles them 90° per press.**
([[D043 - Orientation is the clock's job; apps borrow landscape]])

- Portrait clock: hours over minutes, no colon — Fliqlo's own phone layout.
  `ui_init` is re-entrant; a live change rebuilds the clock for the new shape.
- The drawer wraps three tiles wide in portrait. Apps stay 600×450 and the
  host lends them the nearest landscape, restoring on the way out.
- Portrait line mode routes through the no-canvas fade — the zoom renders the
  cards side by side, and 572 px cannot fit 450.
- `settings.rotation` (0–3) migrates from the old `rotate_180` bool. The
  Settings switch became a Rotation selector applied on Save; BOOT is the
  live control. `POST /rotate {"deg":..}` drives it over HTTP; `/health`
  reports `rotation`.
- Verified remotely through all four orientations twice, plus
  portrait→Timer→clock forcing and restoring landscape, uptime climbing
  throughout.

## v1.21.1 — 2026-08-23

**The chip no longer vanishes when USB is unplugged — which was the bug, and
it was inverted exactly wrong.** The board's `isBatteryConnect()` is
implemented as `getVbusVoltage() != 0` (the real SY6970 detect is
`error("Not implemented")` in XPowersLib), so "battery present" actually meant
"USB plugged in", and the indicator hid the moment the device ran on the
battery it was indicating. Presence is now judged from cell voltage
(2800–4400 mV, debounced over two reads). D042 carries the addendum.

Also: **the chip is white now** — border, nub and number in the digits' white,
per the owner. Green (charging) and red (< 15 %) stay: those are information.

## v1.21.0 — 2026-08-23

**A battery chip on the clock, and battery telemetry in `/health`.**
([[D042 - The battery gauge is a voltage estimate]])

- 46×22 battery glyph, top right, percentage inside. Grey normally, green on
  the charger, red under 15 %. **Hidden when no battery is connected** — the
  normal state of this device — and in line mode, where the weather strip owns
  that corner. Rides the burn-in walk like everything else.
- The SY6970 has no fuel gauge, so percent is a piecewise resting-curve
  estimate from cell voltage: low under load, optimistic while charging (the
  CV phase sits at 4.2 V long before full). The green is partly a disclaimer.
- Polled every 30 s from the loop task — the PMU shares the internal I²C bus
  with touch, so the bus keeps a single master (D018's rule, applied to
  wires).
- `/health` gains `battery{present, mv, pct, charging, vbus}`; `pct` is −1
  with no battery.

Telemetry, not power management: no low-battery shutdown, no deep sleep.

## v1.20.4 — 2026-08-22

`GET /health` reports `current`, the screen now showing — promised by D039 and
not actually wired until now.

## v1.20.3 — 2026-08-22

**Back inside a Lua app rebooted the device.**
`l_back()` called `app_host_back()` synchronously, from a C function running
inside `lua_pcall` on `L` — and that path reaches `script_destroy()` →
`lua_close(L)`. The interpreter was closed by code it was running. Bindings now
*request* a lifecycle change and the host serves it on the next tick, once the
Lua call has unwound. ([[D041 - Nothing may unwind through a live interpreter]])

**The last drawer tile was a nameless blank square.** Two stacked causes: the
wrapping row was 300 px against a 330 px need, clipping the second row by 30 px
— and the name label sits in the bottom 20 px of a cell. Plus script apps have
no icon, because the `App` contract allows null and no `.lua` can draw one. The
row is now 332 px (the full space between the title and the Clock button), and
icon-less apps show the first letter of their name.

**`blink.lua` is a bench tool now** — pick any output-capable pin on the device
with ◀ ▶ and start it; changing pins releases the old one first. GPIO0 is
deliberately absent from its list (D036). **`uptime.lua` removed** at the
owner's request.

## v1.20.1 — 2026-08-22

**Fixes the reboot on launching a Lua app.** Two defects, either of which
alone was fatal. ([[D040 - A script must not be able to reboot the clock]])

`luaconf.h` tests `LUA_USE_C89` with `defined()`, so the library's
`-DLUA_USE_C89=0` — added to "be explicit" — switched C89 mode **on** for
`lib/lua` and only for `lib/lua`, since `library.json` flags do not reach the
consumer. `lua_Integer` was 4 bytes inside the library and 8 in
`src/script.cpp`, so `luaL_checkversion` correctly reported different numeric
types. Both flags removed.

That error was raised inside `luaL_requiref`, outside any `pcall`, where Lua's
only option is `abort()`. So *any* unprotected Lua API error rebooted the
device — in a runtime whose stated promise was that a broken script fails
alone. `lua_atpanic` now longjmps back into `script_create`, which shows the
message and closes the state.

Verified by launching both scripts remotely: uptime kept climbing.

## v1.20.0 — 2026-08-22

**`POST /launch` opens any screen without a finger.**
`{"name":"Blink"}`, or `clock` / `drawer`. The request is recorded on the web
server's task and served on the LVGL loop, so D018's one rule still holds.
([[D039 - The device must be drivable without a finger]])

Built because the reboot above could not be reproduced from a shell. It turned
four guesses into one command and a same-build core dump that decoded to the
exact line on the first try.

## v1.19.2 — 2026-08-22

**`/crash` now proves whether its own backtrace can be trusted.** The first
real capture decoded into a stack mixing `build_drawer` with `l_ui_label` —
functions that never call each other — because the ELF had been rebuilt since
the crash.

The dump carries the first 16 hex chars of the crashing image's ELF sha256.
The endpoint compares it with the running image's and reports `same_build`;
`tools/crash` refuses to decode when they differ rather than printing
confident nonsense. What stays trustworthy either way is the reset reason and
the IDF-region frames, whose addresses do not move between builds.

This turns D038's written warning into an enforced check.

## v1.19.1 — 2026-08-22

**Fixes the reboot when scrolling the app drawer, and makes the next crash
readable without a cable.** ([[D038 - Crashes must be readable without a cable]])

### The drawer could not show five apps

One flex row held 3 comfortably and broke silently at 5: 678 px of tiles in a
600 px row, overflow drawn off-screen with no way to reach it. **That is why
scrolling was being attempted at all.** It now wraps and scrolls vertically —
vertically only, so it cannot fight the left-edge back gesture for the same
finger movement.

### Deleting a screen mid-scroll was a use-after-free

LVGL's `obj_del_core()` clears `act_obj`, `last_obj` and `last_pressed` when an
object dies — **but never `scroll_obj`**. Delete a screen while a finger is
scrolling on it and the input device keeps a pointer into freed memory, which
the scroll-throw handler dereferences on the next read. `release_input()` now
runs before every screen deletion.

### Two more in the same path

- **A drag in the drawer launched an app.** `LV_EVENT_CLICKED` fires on any
  press-release over a tile; the clock has disqualified moved presses since
  D027, the drawer never did. The same flag stops a scroll being taken as a
  reorder.
- **Script uploads rebuilt the registry from the web server's task**, racing
  the LVGL task's `app_at()` calls and invalidating tile `App` pointers if the
  drawer was open. Uploads now mark it dirty; the rebuild happens on the LVGL
  task while nothing is open. `/apps` gained a `pending` flag.

### `GET /crash`

The `coredump` partition and `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` have been
there since Stage 0 — panics have been writing full ELF core dumps all along.
They needed reading, not enabling. The endpoint reports the reset reason, the
faulting task, PC, exception cause and a 16-deep backtrace; `tools/crash`
decodes it to file and line with `addr2line`. `DELETE /crash` clears it.

RAM 19.1 % · Flash 30.0 %.

## v1.19.0 — 2026-08-22

**Apps can be added and removed without restarting.** A Lua 5.4 runtime sits
beside the native apps; a script is `/apps/<name>.lua` in LittleFS and appears
in the drawer like anything else. ([[D037 - Apps become Lua scripts]])

```bash
tools/app add apps/blink.lua Blink     # under a second, no reboot
tools/app list
tools/app rm Blink
```

- **Lua 5.4.7 vendored** in `lib/lua/`, not pulled from the registry. Measured
  cost: **+148 KB flash (27.3 % → 30.0 %), +1 KB RAM.**
- **A fresh `lua_State` per launch**, closed on exit — "create on entry,
  destroy on exit" holds for scripts exactly as for native apps.
- **Lua allocates from PSRAM**, never the internal heap the display driver
  needs.
- **An instruction budget stops runaway scripts.** Scripts run on the LVGL
  task, so `while true do end` would otherwise freeze the clock with the cable
  as the only way out — the one failure this feature must not have.
- **Compiled at upload, not at launch**: a syntax error is rejected with the
  parser's own message and any previous working version of that name survives.
- **`io`, `os`, `package` and `debug` are not loaded**, and GPIO bindings
  enforce the Lab's pin rules including GPIO0 being readable, never drivable.
- The registry became `app_count()`/`app_at()`; a stale `app_order` now falls
  back to identity for the whole list rather than hiding an app.
- Two examples ship in `apps/`: `blink.lua` and `uptime.lua`.

No reboot is needed to see a new tile, and that falls out of
[[D033 - Back goes one level, not home]] — the drawer is rebuilt on every visit,
so it re-reads the registry each time.

## v1.18.1 — 2026-08-22

**The Lab's Back button moved to a bottom bar.** It had floated at the top
right, over the tab-button row — which put it on top of the right end of the
**UART** tab, so aiming for UART hit Back instead.

Anywhere in the top 46 px collides: the tab row spans the full width by
construction. The bottom is also where Timer and Settings already keep Back,
so this fixes an inconsistency as well as a collision. The tabview now ends at
398 px, leaving a 52 px strip.

Each tab was checked against the smaller area rather than assumed: UART's
monitor ends at 306 of 338 usable, I²C's results label starts at 120 and that
tab scrolls, GPIO scrolled already. The strip also carries a dim reminder that
leaving returns every pin to Hi-Z — said where the leaving happens.

## v1.18.0 — 2026-08-22

**GPIO0 joins the Lab, input-only.** The BOOT button is now visible live —
Hi-Z reads **H** at rest and **L** while pressed, thanks to its external 10 K
pull-up. ([[D036 - GPIO0 is readable, never drivable]])

- **No Out mode, and the dropdown simply omits it.** The BOOT button is
  hard-wired from GPIO0 to ground: an output driving high is one press away
  from shorting the pad through the button. `apply_mode()` refuses it a second
  time in code, because the UI is one edit away from being wrong.
- **Bus dropdowns gained a `bus_map[]`.** They index pin lists by dropdown
  position; adding an entry at the front of `PINS[]` would have shifted every
  I²C and UART selection by one and scanned the wrong pins with no error.
  Input-only pins are absent from those lists via the map rather than by an
  ordering convention that would break silently later.
- Bus defaults unchanged — the map skips exactly the one added pin.

## v1.17.0 — 2026-08-22

**The Lab: the clock is now a bench tool.** Third app in the drawer — GPIO
control, I2C scanner, UART monitor. ([[D035 - The Lab may only touch pins the firmware does not own]])

- **GPIO** — the 14 header pins the firmware does not own (21, 38–42, 47, 48,
  TX0/RX0, and the four SD pins, labelled). Hi-Z / pull-up / pull-down /
  output per pin, live level readout, tap to toggle outputs.
- **I2C** — scanner on any two whitelist pins via `Wire1` (default 47/48,
  adjacent on the header), 100/400 kHz, plus a read-only scan of the internal
  6/7 bus — it lists the PMU, the touch controller, and anything on the P4
  plug.
- **UART** — `Serial1` on any pin pair (default 43/44), 9600–230400, live RX
  monitor with a 512-byte tail, canned test sends.
- **Leave no trace:** leaving the app returns every pin to Hi-Z and releases
  both peripherals.
- The whitelist came from the schematic cross-checked against the board
  config. GPIO18 is on the header and still excluded — it is the display's TE
  line. On the header ≠ free.

First feature to ship entirely over the air.

RAM 18.7 % · Flash 27.7 %.

## v1.16.1 — 2026-08-22

Version bump only. Exists to be the first release delivered over the air —
the proof that v1.16.0's mechanism works end to end, confirmed by `/health`
reporting this version with a fresh uptime.

## v1.16.0 — 2026-08-22

**Updates ship over the air.** `POST /update` takes a raw firmware image and
writes it to the inactive OTA slot; `tools/ota` builds, pushes, and refuses to
call it done until `/health` reports the new version.
([[D034 - Updates ship over the air]])

- **No repartition was needed** — `default_16MB.csv` has carried `ota_0`,
  `ota_1` and `otadata` since Stage 0. The second slot was always there, which
  is why this is a MINOR release and settings survive untouched.
- Raw body, not multipart: `Update.begin()` gets the exact size, and the
  ESP32 magic-byte check rejects a wrong file in the first chunk.
- Reboot on client disconnect, so the 200 provably reaches the caller first.
- The device narrates its own update — `flashing` on the line while it writes,
  `error` if it fails — through the same queue as every other transport.
- Deliberately absent, and written down as such: authentication (private LAN,
  accepted; header-token hook noted for the day it matters), rollback (stock
  Arduino core cannot; **the cable remains the rescue path**), resume.

This release itself still crossed the cable — the running firmware had no
`/update` yet. v1.16.1 exists to be the proof.

RAM 18.3 % · Flash 27.3 %.

## v1.15.0 — 2026-08-21

**Back means back.** Navigation is a stack — app → drawer → clock — and every
way out pops one level instead of jumping home.
([[D033 - Back goes one level, not home]])

- **`app_host_back()` is the one door.** The left-edge swipe, the timer's new
  back card, Settings' bottom-bar button and the drawer's Clock button all
  route through it. The app-first-refusal hook is unchanged, so Settings with
  its editor open still closes the editor and goes nowhere.
- **The timer has a visible back button again** — a third card in the control
  row (back · Start · Reset, 120/202/202). v1.14.0 had left the invisible edge
  swipe as its only exit.
- **Settings' "Close" is now "Back"** and lands on the drawer, not the clock.
  The unsaved-changes guard is untouched.
- **The drawer is rebuilt on every visit** — backing out of an app calls the
  same `build_drawer()` swipe-up uses. Nothing resident, PSRAM flat, no stale
  copy when the app order changes.

Supersedes the destination half of D029; the polling architecture stands.

RAM 18.3 % · Flash 27.2 %.

## v1.14.0 — 2026-08-18

**The 3-second hold is gone, the drawer no longer reboots, and the timer is
usable.**

### Retired: hold to open Settings

Settings is an app and the drawer is how you reach apps — the hold was a second
route to a place that already had one, and it fired when you rested a finger on
the clock. Gone, with the accent bar that advertised it. The clock is down to
three gestures. ([[D030 - Retire the 3-second hold]])

### Fixed: long-pressing a drawer tile reset the device

The handler deleted the active screen from inside an event callback on one of
that screen's own grandchildren, then rebuilt it — so `lv_disp_t.act_scr` was
left dangling and the first `lv_obj_create()` of the rebuild invalidated through
it. Structural, not a race.

Nothing needs deleting: the tiles are flex children, so `lv_obj_move_to_index()`
*is* the reorder. The layout is the model.

The same handler would then have launched the app it had just moved — LVGL sends
`LV_EVENT_CLICKED` on every release, including the end of a long press.
([[D031 - The layout is the model]])

### Timer

- **A split seam across each card**, so it reads as the same object as the clock.
- **Three ways to set a number, on the same card**: drag for coarse (18 px a
  step, calmer than before), tap above or below the middle for exactly ±1, hold
  for ±1 repeating at ~10/s. One control could not be both fast over range and
  precise; three gestures can. Seconds wrap, minutes clamp, and the fold runs at
  70 ms a phase so the animation stops lagging the finger.
  ([[D032 - Three gestures, one control]])
- **Start and Reset are cards now** — full card width, 122 px tall, montserrat
  32, sitting directly under the numbers they act on. No seam: that line means
  *this flips*, and through a word it reads as a strikethrough.
- **No frame around a running number.** A coloured border reads as an error box;
  the button already says Pause.
- **The Clock button is gone.** The left-edge swipe is the way out, with a thin
  dim bar at the edge so the gesture has an affordance.
- Pressing **Start** on a finished timer now starts it, rather than only
  clearing the alarm and waiting to be pressed again.

RAM 18.3 % (59 924 B) · Flash 27.2 % (1 785 665 B).

## v1.13.0 — 2026-08-17

**Swipe in from the left edge to go home**, from any app and from the drawer.

It is polled from the LVGL loop rather than handled as a screen event, because
LVGL 8 does not bubble events and every clickable child — the timer's cards, the
settings tab bar, its lists and rollers — would have eaten the press. Polling
puts it above the widget tree, which is where a system gesture belongs: apps get
it for free and none of them can break it.
([[D029 - Back is a system gesture, not a widget event]])

- **Armed by where it starts** (44 px of the left edge), **judged on release**
  (110 px rightward, 90 px of vertical wander allowed) — the same shape as the
  clock's swipe-up, so the two gestures feel like one system.
- **The app gets first refusal.** `App` gains an optional `back()`. Settings
  uses it to close its text editor instead of leaving, which is why a stray edge
  swipe can no longer discard a hand-typed Wi-Fi password. The field is additive;
  five-field initializers still compile.
- **The timer roller stands down for it.** A drag that turns mostly sideways is
  abandoned *and the value is put back* — the minutes card overlaps the edge
  zone, and `set_seconds` outlives the screen, so without this you could come
  back to a timer set to a number you never chose.

The per-app **Clock** buttons stay. The gesture is invisible; the button is the
discoverable path.

RAM 18.3 % (59 916 B) · Flash 26.5 % (1 739 597 B).

## v1.12.0 — 2026-08-17

**Settings became an app, and the timer got rollers that flip.**

- **Settings is an app now**, sitting next to Timer in the drawer. The 3-second
  hold still opens it — that gesture is muscle memory and was not worth
  spending. `app_settings.cpp` is a thin adapter over the existing screen, so
  the settings UI itself did not change at all.
- **Apps are reorderable.** Long-press a tile in the drawer and it moves one
  place left, wrapping at the front. The order lives in NVS (`app_order`), so
  it survives a power cycle and a re-flash like every other setting.
- **The timer is set with one roller per number**, matching Settings ▸ Night
  from. Drag the minutes card for minutes, the seconds card for seconds. This
  replaces dragging the whole face, which could only ever change one quantity
  and gave no clue which.
- **Timer digits fold.** Every change plays the clock's two-phase split flap,
  so the timer reads as the same object as the clock rather than a different
  app that happens to show numbers. ([[D028 - Set a number by dragging the number]])

The value is computed from total displacement since touch-down rather than
accumulated per event, so it cannot drift, and dragging back to where you
started restores the number you started with. A card already mid-fold takes the
new value without queueing another animation — stacked folds read as tearing.

RAM 18.3 % (59 900 B) · Flash 26.5 % (1 739 325 B).

## v1.11.0 — 2026-08-17

**An app platform, and a timer.** Swipe up from the bottom edge for a drawer.

- `app_api.h` — a five-field `App` contract: name, icon, create, destroy, tick.
- `app_host.cpp` — registry, drawer, launch, route home. Knows nothing about
  what any app draws; no app knows anything about the clock.
- `apps/app_timer.cpp` — drag up or down to set, play / pause / reset / back.
  Same cards, same 210 px digits: a second visual language on one device would
  be one too many. Coarse above ten minutes (whole minutes) and fine below
  (15 s), so a 45-second egg and a 40-minute bake are both one gesture.
- Adding an app is now **one file plus one line** in `app_registry.cpp`.

**The gesture, and why it does not break the other three.** A swipe *begins as a
press*, so displacement now disqualifies a press whatever its duration —
otherwise an upward drag would also fire a tap, or a long press that silently
changed the brightness on the way to the drawer. The swipe must also start
within 80 px of the bottom edge, and the hold indicator is suppressed the moment
movement starts. See [[D027 - The gesture budget]].

*Found while linking:* `const App app_timer = {...}` had **internal linkage** —
in C++ a const object at namespace scope is TU-local by default, so the registry
could not see it. `extern` on the definition is required, not decoration.

RAM 18.3 % (59 868 B) · Flash 26.5 % (1 737 913 B).

## v1.10.1 — 2026-08-17

**Fix: "weather 496370 h ago".** That figure is 56.6 years — the age of the Unix
epoch, and exactly `now / 3600`.

Weather and NTP both start on `GOT_IP` and race. Weather usually wins, so the
fetch was stamped with a near-zero epoch; when NTP then stepped the clock to
2026, `now - last_sync` measured the distance back to 1970 rather than the age
of the reading.

Age is now measured with **`millis()`**, which cannot be stepped, and `uint32`
subtraction wraps correctly at the ~49.7-day rollover so it stays right across
that too. All three call sites — the tap overlay, `GET /health`, and
Settings ▸ Info — go through one `net_weather_age_s()` accessor, so the bug
cannot be reintroduced in one place while being fixed in another.

Verified across a boot: `-1` (never) → `7 s` → `20 s`.

## v1.10.0 — 2026-08-17

**Brightness can follow the sun instead of the clock.**
`Settings ▸ Screen ▸ Dim by sunrise` — **on** uses sunrise/sunset from the
internet, **off** uses the fixed night window. Default on.

The fixed window was wrong every morning between sunrise and the window's end.
Caught exactly that way: at **06:10** with sunrise at **05:58**, the sun was up
and the panel was still at night brightness, because the configured window ran
to 07:00. The schedule was doing what it was told; it was simply wrong about
the world.

Open-Meteo already supplies `is_day` for the weather icon, derived from
sunrise/sunset at the exact coordinates — so this costs nothing, tracks the
seasons, and follows the city if it changes. The hour window remains the
**fallback** whenever weather has never arrived, so a device with no network
still dims sensibly.

`GET /health` now reports `brightness: {day, night, follows_sun}`, so what is
actually persisted can be checked without opening Settings.

## v1.9.0 — 2026-08-16

**The panel can be flipped 180°.** `Settings ▸ Screen ▸ Flip 180`, applied live
so you can judge an orientation while looking at it, and persisted to NVS.
Defaults on, since that is the way the clock actually sits.

Rotation **2** is the other landscape: identical 600×450, and the library
re-applies the CST226SE swap and mirror with it, so touch follows the screen.
Because the dimensions are unchanged, LVGL needs no re-init and this is a live
toggle rather than a reboot.

*Fixed during the flash:* the first build boot-looped with `LoadProhibited` at
`0x8`. `app_apply_rotation()` repaints via `lv_scr_act()`, and the boot call
happens **before** `beginLvglHelper()` — so LVGL was uninitialised and
`lv_scr_act()` was NULL. The repaint is now guarded on `lv_disp_get_default()`;
only the runtime toggle needs it, since nothing has been drawn at boot.

## v1.8.0 — 2026-08-16

**A moon after dark.** The clear-sky icons follow the sun: sunny by day, a
crescent by night.

Day/night comes from Open-Meteo's **`is_day`**, which the API derives from
sunrise and sunset for the exact coordinates — so the device never does solar
geometry or reasons about the timezone, and it stays correct when the city
changes. Verified against the same response: sunrise 05:57, sunset 18:56,
`is_day: 0`.

Only `clear` and `partly cloudy` change. A cloud looks the same after dark, and
every weather UI worth copying leaves the rest alone.

The crescent is carved by drawing the card colour back over an offset disc —
cheaper and crisper than arc maths, though it only works because the icon sits
inside a card of known colour.

`is_day` added to `GET /health`.

Includes **v1.7.6**: the red charge LED on the back stops blinking. It was never
reporting power — with no battery the SY6970 cannot complete a cycle, and a
blink is how it reports that fault.

## v1.7.6 — 2026-08-16

**The red LED on the back stops blinking.** `beginAMOLED_241(..., disable_state_led=true)`.

It was never reporting power, as D007 assumed — it was reporting a **fault**.
There is no battery, so the SY6970 can never complete a charge cycle, and a
blinking red LED is precisely how it says so. Correct behaviour describing a
condition that will never change and that nobody needs to know about. Charging
is unaffected, because there is nothing to charge.

## v1.7.5 — 2026-08-16

**Fix: the temperature disappeared from both screens.** `mini_card()` never
recorded the card height it was given, so `card_sync()` sized the temperature
card to **zero height**. Humidity survived only because its branch had a
`?: fallback` that the temperature branch did not.

The height assignment was written in v1.7.0 but **never applied** — the
scripted edit's match string did not account for a blank line before the
closing brace, and a failed string replace fails *silently*. The bug then sat
dormant for four releases because nothing depended on `c.h` until min/max moved
into the temperature frame.

Two changes as a result: the assignment is in, and the temperature branch now
carries the same `?:` fallback, so a zero can never make a card invisible again.

## v1.7.3 — 2026-08-16

**min/max moves inside the temperature card.** One frame now carries the current
reading and today's range together:

```
┌──────────────────────────┐  ┌─────────┐
│  ☁   40°   31/41         │  │   37%   │
└──────────────────────────┘  └─────────┘
```

At 22 px and back to grey `#8A8A8A`, dropped 8 px so it sits low against the big
number — the way a range reads beside a headline figure. It is context *for* the
temperature, not a peer of it, and putting it in the same frame says so
structurally rather than relying on size alone.

The card measures itself from both labels, so it shrinks when line mode drops
the range and grows back when the clock returns.

## v1.7.2 — 2026-08-16 *(superseded)*

Read "pallet" as *palette* and made min/max white. It meant **frame** — see
v1.7.3.

## v1.7.1 — 2026-08-16

- **min/max at 22 px — exactly half the 44 px live readings**, so the hierarchy
  is a stated ratio rather than a guess. Card height 32.
- **`FW_GIT` build stamp that cannot go stale.** `FW_BUILD` uses
  `__DATE__`/`__TIME__`, baked in when *main.cpp* is compiled — change any other
  file and the stamp stays put while the binary genuinely changes, which is the
  exact failure it existed to catch. Caught it live: v1.7.0's fix to `net.cpp`
  flashed a different image under an unchanged stamp.
  `scripts/version_stamp.py` injects the git revision as a build **flag**, so a
  change forces a rebuild and names the source precisely. `+dirty` marks
  uncommitted work. Reported in the boot log and `GET /health`.

## v1.7.0 — 2026-08-16

**Weather icon, and a size hierarchy in the weather row.**

- **Temperature and humidity stay at 44 px; min/max drops to 34 px.** They are
  the live readings; min/max is a forecast you consult. Before this the min/max
  card was the *widest* thing in the row — the least important value dominating
  it.
- **A weather glyph left of the temperature**, inside the same card. Eight icons
  — clear, partly cloudy, overcast, fog, drizzle, rain, snow, thunderstorm —
  folded from Open-Meteo's 28 WMO 4677 codes. Severity is dropped deliberately:
  at 40 px "light drizzle" and "dense drizzle" are indistinguishable, and a
  glance only needs the kind.
- Drawn with **primitives, not bitmaps**: eight bitmaps would be ~13 KB of
  flash, and drawing them means they inherit the palette and stay crisp at any
  size. Monochrome, like everything else.
- `weather_code` added to `GET /health`.

Fixed while testing: `memset` left the code at **0**, which is WMO "clear sky",
so a sun would show before the first fetch ever succeeded. Now −1 = unreported,
which falls back to the neutral cloud.

RAM 18.2 % (59 796 B) · Flash 26.5 % (1 734 709 B).

## v1.6.0 — 2026-08-16

**The caption says what is happening, and is quiet about it.**

- **60 % brightness**, matching the clock and temperature. Pure white at
  `DIM_OPA` rather than a baked grey, so it is literally 60 % of the line —
  which stays the only thing at full brightness.
- **Renders `state` + `message`.** The line's motion carries the verb, but only
  if you can read amplitude. `ui.cpp` alone never said whether it was being
  read, edited or debugged; **building  ui.cpp** does.
- **Message limit 20 → 48 chars**, wrapping to two lines at 560 px, so a real
  phrase fits instead of being cut.
- **A state with no message shows the state name**, so the bottom of the screen
  is never blank while something is happening.

Convention recorded in `CLAUDE.md`: `message` is an **object**, never a verb —
`"LVGL canvas docs"`, not `"reading"`, which would render "reading reading".

RAM 18.2 % (59 740 B) · Flash 26.4 % (1 730 637 B).

## v1.5.0 — 2026-08-16

**Line mode revised: the corners recede so the line is the subject.**

- **Colon restored on the corner clock** — and in the zoom canvas. It was
  missing from both, so the "true scale" was not faithful and the colon
  *vanished the instant the flight began*. A defect, not a preference.
- **Temperature only while the line runs.** Min/max and humidity are reference
  figures you consult deliberately on the resting screen; while the line is up
  you are watching the line. 377 px of weather becomes 86 px. They return with
  the clock.
- **Corners dim to 60 %** during line mode. On true black this costs almost no
  light and reads as depth rather than as something switched off. The zoom
  canvas fades to the same value on the way in, so the hand-off to the corner
  cards stays invisible.

RAM 18.2 % (59 716 B) · Flash 26.4 % (1 730 513 B).

## v1.4.3 — 2026-08-16

Seam back to **3 px** on the big clock. Small cards still have none.

## v1.4.2 — 2026-08-16

**Seam back on the big clock only, and thinner: 3 px → 2 px.**

The v1.4.1 removal went one card too far. At 232 px the seam reads correctly —
2 px is a hairline that sits cleanly and says split-flap. What it does not
survive is being *scaled*: on the 55 px corner clock and 58 px weather cards
there are too few pixels for a line to land on, which is what looked choppy.

So the rule is now scale-dependent rather than all-or-nothing:

| Card | Height | Seam |
|---|---|---|
| Clock | 232 px | **2 px** |
| Weather | 58 px | none |
| Corner clock | 55 px | none |

The zoom canvas carries the seam, since it renders the big clock. It scales
with everything else and is sub-pixel by the time the seamless corner cards
take over, so the handover stays invisible.

RAM 18.2 % (59 716 B) · Flash 26.4 % (1 730 229 B).

## v1.4.1 — 2026-08-16

**Seam removed from every card.** The original brief specified a horizontal
centre seam and it had been there since Stage 1 — but once the same card
language was applied at 55 px and 58 px as well as 232 px, a 2 px black line
across a 55 px card has too few pixels to sit cleanly and reads as choppy
rather than as a split-flap gap.

**The fold animation is untouched.** It still hinges at `CARD_H/2`. The seam was
only ever a drawn hint of where that hinge is, and the fold itself shows it
better than a static line did.

Also removed the per-card `accent` object — a transparent child on every card,
dead since the v4 line renderer stopped pulsing it.

RAM 18.2 % (59 708 B) · Flash 26.4 % (1 730 117 B).

## v1.4.0 — 2026-08-16

**The two halves of the transition are now strictly sequential.**

```
in    scale the clock into the corner   ->  THEN the line appears
out   the line disappears               ->  THEN the clock scales back
```

They used to overlap, which read as two unrelated things happening at once. In
sequence it reads as one movement: the clock gets out of the way, and the line
takes the space it vacated.

- Entering, a one-shot timer fires at the end of the flight and only then
  creates the line and its caption.
- Leaving, the fade-out drives the hand-off — when the line's gain reaches zero
  the wave timer itself triggers the fly-back, so the order cannot slip.
- **A state change while the line is already up does not re-run the transition.**
  Activities change every few seconds; re-flying the clock each time would be
  unwatchable. Only the caption swaps.
- A new emotion arriving mid-fade-out brings the line straight back without a
  spurious scale, since the clock is already parked (`layout_small`).
- Clearing mid-flight, before the line ever appeared, just flies back.

RAM 18.2 % (59 740 B) · Flash 26.4 % (1 730 385 B).

## v1.3.0 — 2026-08-16

**The clock now genuinely scales.** Entering line mode no longer cross-fades
between two clocks — the real clock is scaled down and flown to the top-left
corner while the weather slides to the top-right.

LVGL 8 cannot transform text, so the clock is rendered into an `lv_canvas`
(which derives from `lv_img`) and *that* is zoomed: 265 KB in PSRAM. At the end
of the flight it hands over to real 50 px cards whose geometry is the big
clock's multiplied by the same 0.2381 factor — card 64×55, radius 6, gap 9,
digit padding 4.1 px both ways — so the handover is invisible and the digits go
back to being real glyphs rather than a downscaled bitmap.

Weather "just moves", as asked: same 44 px cards, right-aligned against the far
edge by a **measured** shift, since the row's width changes with its values.

Falls back to the previous cross-fade if PSRAM cannot provide the canvas.

RAM 18.2 % (59 716 B) · Flash 26.4 % (1 730 129 B) · PSRAM −268 KB.

## v1.2.0 — 2026-08-16

**Save and Close are separate, with a guard.** One "Save & close" button gave no
way to back out — and that mattered more than it looks, because the brightness
sliders preview **live**: a user who dragged one and then left had already
changed the device with nothing to undo it.

- **Save** commits and stays open, confirming with a green "Saved" for ~2 s.
- **Close** with unsaved edits raises *"Close without saving?"* —
  **Discard** / **Keep editing**.
- **Discard** restores the snapshot taken when the screen opened and undoes the
  live effects (brightness, timezone, weather visibility). NVS is untouched, so
  nothing needs rewriting.
- **Save lights up blue** whenever there is something unsaved, so the dialog is
  never a surprise.

Sliders no longer write into `settings_get()`; they preview only, and the
committed value is read from the widget on Save. Without that change Discard
would have had nothing to restore.

The explicit apply-now actions — Connect, city pick, Apply & sync NTP, Reset —
take a fresh snapshot, so deliberate applies never read as unsaved work.

RAM 18.2 % (59 732 B) · Flash 26.3 % (1 725 333 B).

## v1.1.0 — 2026-08-16

**Every value now wears the same card.** The resting weather block changed from
plain text labels to three charcoal cards with centre seams at 44 px, matching
the clock's design language. Hierarchy is carried by scale — the clock is 4.8×
the type size — and by colour, rather than by two different treatments on one
screen.

- New face `fliqlo_mid` (44 px, `0-9 ° / %`) for the weather cards, alongside
  `fliqlo_small` (38 px) for the line-mode strip. Two scales so the strip stays
  visibly subordinate.
- `mini_card()` is now scale-parameterised and shared by both rows.
- Weather sits at y 330, centred in the space the clock cards leave.
- Stale marker is a 10 px dot beside the cards — still a shape, never a type
  size.

Flash usage **fell** 92 KB despite two new faces: dropping the last reference to
`lv_font_montserrat_48` let the linker discard it.

RAM 18.1 % (59 468 B) · Flash 26.3 % (1 723 357 B).

## v1.0.0 — 2026-08-16

First tagged release. Feature-complete against the original brief, plus
everything added during the build.

**Clock.** Fliqlo split-flap on true black, 600×450 landscape, a purpose-
generated 210 px digit font with tabular advance, two-phase fold animation on
minute change, NTP with a POSIX timezone, daily resync.

**Weather.** Open-Meteo on a dedicated FreeRTOS task — current temperature,
today's min/max and humidity, 15-minute refresh, capped exponential backoff,
stale marker, and never on the render path.

**Line display.** 32 states as points in Russell's circumplex: arousal drives
amplitude, frequency and speed; character drives shape. One continuous white
polyline built from three incommensurate harmonics with a speech envelope and
an end taper. Reachable over HTTP, BLE NUS and a BLE HID output report.

**Settings.** On-device, opened by a 3-second hold: Wi-Fi scan and join,
timezone, city search, brightness and night hours, BLE identity, diagnostics.
Persisted to NVS and survives re-flashing.

**Bluetooth.** Custom vendor HID identity so macOS lists and pairs the device
without mistaking it for a keyboard, with a keyboard descriptor kept as a
compatibility fallback.

**Care.** Brightness schedule, ±2 px burn-in walk, tap and long-press gestures.

**Measured:** cold boot to correct local time **4.45 s** against a 10 s budget;
emotion auto-revert at 5.2 s; malformed API input returns 4xx with no reboot;
Wi-Fi loss degrades gracefully and reconnects with backoff.

RAM 18.1 % (59 444 B) · Flash 27.7 % (1 815 465 B).
