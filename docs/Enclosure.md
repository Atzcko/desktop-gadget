---
title: Enclosure
type: project
tags:
  - project
  - hardware
  - fusion360
updated: 2026-08-22
---

# Enclosure — the Fusion 360 side

The physical half of the project lives in Autodesk Fusion, reached through
the **Autodesk Fusion MCP** (available when Fusion is running on the Mac).

## Where things are

| | |
|---|---|
| Fusion project | **Desktop Gadget** (created 2026-08-22) |
| Working file | **ESPRT01 Case** — copied from v120 of the original in the ESPRT01 project |
| Lineage ID | `urn:adsk.wipprod:dm.lineage:wLSqvPsrQPmPhvuT1j7LWg` |
| Original (untouched) | ESPRT01 project, same name, own lineage |

The copy is its own lineage: edits in Desktop Gadget never touch the ESPRT01
original. The source project also holds **ESPRT01 Case Battery Lid** (not
copied) and **T4 AMOLED CASE** — an existing enclosure for this exact board,
briefly copied by mistake and removed, worth remembering as prior art.

> [!warning] Fusion's document search silently caps at 15 results
> Searching "ESPRT01" returned 15 of the project's 27 files and the Case was
> not among them; a name-scoped search then matched the wrong file. **Enumerate
> `project.rootFolder.dataFiles` instead of trusting search** — the same
> silent-truncation genre as the unmatched `str.replace` (see log, 2026-08-16).

## Constraints the case inherits from the firmware

- **BOOT must be pressable in DAILY USE, not just at plug-in** — since
  v1.22.0 it is the orientation button (90° per press, D043), on top of
  being the flash rescue path if an OTA image crash-loops
  ([[D034 - Updates ship over the air]]).
- The 2×15 header carries the [[D035 - The Lab may only touch pins the firmware does not own|Lab whitelist pins]];
  an enclosure that blocks it turns the bench-tool app into decoration.
- USB-C is the only power. No battery logic exists in firmware.

## Print files

The exported meshes live in the repo at `3D print/` (added 2026-09-05,
exported from Fusion 08-26): `DG_Case.stl` (17.6k triangles),
`DG_Buttons.stl` (8.6k), `DG_Switch.stl` (6.4k) — binary STL, 1.6 MB
together. The case has left CAD and is headed for a printer, which makes
the BOOT-reachability constraint above load-bearing.

## Related

- [[T4-S3]] — the board the case must fit
- [[index]]
