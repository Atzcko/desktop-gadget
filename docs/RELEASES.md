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
