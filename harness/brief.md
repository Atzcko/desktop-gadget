---
title: Brief
type: brief
status: draft
project: Desktop gadget
created: 2026-09-13
updated: 2026-09-13
---

# Brief — Desktop gadget

> [!important] Set `status: ready` only when "Definition of done" and "Must
> never happen automatically" are filled in. The architect refuses to start
> without them.

> [!note] Rehearsal. This harness was set up on 2026-09-13 as a dry run of
> the `harness` skill on a mature project: the Understand and Architect
> phases only, no operating. Everything below that is not marked *(interview)*
> was read from the vault and the knowledge graph, not asked.

## Purpose

Firmware for a LilyGO T4-S3 AMOLED desk clock: a Fliqlo-style flip clock with
weather, and an HTTP and BLE "emotion" API that lets a Claude Code or Codex
session show what it is doing on the desk. Over fifty-odd releases it grew
into an app platform (native apps and live-uploaded Lua scripts) with a Mac
companion for YouTube, a browser, an equalizer, and BLE trackpad and keyboard.
The repository is both the PlatformIO project and an Obsidian vault; every
decision, stage and release is recorded. Status on 2026-09-13: v1.39.x, in
daily use, feature-complete pending the owner's verification of the latest
stage. The last commit added a README and cartridge attribution "ahead of
publishing", which suggests the next stretch of work is publication.

## People

One owner, in Abu Dhabi, who flashes and verifies every stage on the real
device before the next begins (working agreement 3). Claude Code and Codex
are the working agents; `CLAUDE.md` and `AGENTS.md` carry the same
instructions for both. No other reviewer or approver is recorded. How the
owner wants to be talked to and how often: *(interview)*.

## Domains

From the knowledge graph (2779 nodes, 9791 edges, 92 communities on
2026-09-11), collapsed into the seven areas an agent would actually own. God
nodes: `Project log` (169 edges), the vault index, `Releases`, `Module map`,
and `D037 - Apps become Lua scripts`.

| Domain | What lives there | Anchors |
|---|---|---|
| Clock UI and line display | `src/ui.cpp`: flip cards, digit font, fold animation, burn-in walk, orientation, themes; the emotion line engine; health chips | Module map; D014, D019, D049, D059 |
| App platform and host | `src/app_host.cpp`, the drawer, the App contract in `include/app_api.h`, the eleven native apps in `src/apps/`, the Lua runtime in `src/script.cpp` and `/apps/*.lua` | D026, D033, D037, D041 |
| Lua runtime | the vendored VM under `lib/lua`; about half of the graph's communities; third-party and rarely edited | D037, D040 |
| Connectivity and remote surface | `src/net.cpp` (Wi-Fi, weather), `src/httpapi.cpp`, `src/ble.cpp` (HID identity, NUS), messages, OTA update endpoint, crash readback | HTTP API reference; D016, D018, D022, D034, D038, D039 |
| Mac companion | `tools/ytserve`, `tools/eq_capture`, `tools/browser_session.py`, `tools/flipclock.py`, `tools/app`, `tools/ota`, `tools/crash` | D050, D053, D054, D057, D058 |
| Vault and process | 59 decisions in `docs/decisions/`, `log.md`, `docs/RELEASES.md`, `docs/stages/`, `docs/reference/`, `index.md`, the agent instructions | working agreements 1 to 4 |
| Board and display library | the LilyGO AMOLED library, LVGL 8.4.0 pinned, the copied PlatformIO env, the boot order | D001, D002, D003, D005 |

## Definition of done

**For a release** (recorded in `CLAUDE.md`, "Versioning — every release"):
bump `include/version.h`; append the release to `docs/RELEASES.md` with what
changed and why; commit, then tag; flash over the air with `tools/ota`; and
confirm the running version with `GET /health` on the device, never in the
tree. Stages are hardware-gated: the owner verifies on the device before the
next stage begins. `log.md` is appended at the end of any session that
changes the project.

**For a typical task:** *(interview)* — whether "builds, ships over the air,
`/health` reports the new version" is enough for an agent to call a change
done, and which kinds of change need the owner's eyes on the screen.

## Delegation

### Handed off

*(interview)*

### Kept by the owner

Hardware verification of each stage (working agreement 3). The rest:
*(interview)*.

## Must never happen automatically

*(interview)* — candidates read from the vault, to confirm, add to or strike:

- flashing the device, over the air (`tools/ota`) or by cable
- git commit, tag or push in the project
- resetting or changing settings in NVS on the device
- driving GPIO pins through the Lab beyond the whitelist (D035, D036 are
  device-side rules; the question is whether an agent may run Lab actions at
  all)
- sending messages to other gadgets (D046)
- anything that reads the owner's Mac browser session or cookies
  (`browser_session.py`, `ytserve`; D054)
- asking for, or storing, the Wi-Fi password (D016: provisioned on-device,
  never in a file)

## Constraints

Recorded in `CLAUDE.md`, "Hard constraints", and the decisions they cite:

- LVGL 8.4.0, pinned; not 9.x (D002). Board env copied verbatim from the
  library; every flag is load-bearing (D003). No TFT_eSPI (D001).
- PlatformIO runs in a Python 3.12 venv; every shell that runs `pio` needs
  `export PATH="$HOME/.platformio-venv/bin:$PATH"` (D005). The serial port is
  never hardcoded; detect it by USB id `303A:1001`.
- `config.h` holds first-boot defaults only; live configuration is in NVS
  (D013). Wi-Fi is provisioned on the device (D016).
- Position self-aligning widgets with `lv_obj_align()`, never `set_pos()`
  (D019). Every decorative object calls `decor()` (D014). Only the LVGL task
  draws; workers validate, enqueue or flag (D018).
- No deep sleep, no battery logic beyond the gauge; USB-powered, always on.
- Ground truth is the library source at `~/.local/src/LilyGo-AMOLED-Series`,
  not its README.
- Updates ship over the air; the cable is for first install and rescue, and
  there is no rollback (D034). Crashes are read back without a cable (D038).
- Budget and time: *(interview)*.

## Knowledge sources

Where the truth lives when documents disagree: the library source over the
README (working agreement 4); for behaviour, the device over the tree
(confirm on `/health`). To read: `docs/decisions/` D001 to D059,
`log.md` (the narrative), `docs/RELEASES.md`, `docs/reference/` (Module map,
HTTP API, LilyGO AMOLED library), `docs/stages/`, `docs/The OS direction.md`,
and `graphify-out/GRAPH_REPORT.md`. The harness wiki at `harness/wiki/`
compiles these.

## Open questions

The first interview batch, asked 2026-09-13:

1. What counts as done for a typical task, and which changes need the owner's
   eyes on the device?
2. Which of the candidate "never automatically" actions stand, and what is
   missing?
3. What is handed off entirely, what stays with the owner, and what has come
   back wrong before?
4. Budget and cadence: cost ceiling, how often to pull the owner in, and what
   the next stretch of work is.

## Interview log

- 2026-09-13 — harness initialised as a rehearsal; brief pre-filled from the
  vault and the graph; batch 1 asked (the four questions above).
