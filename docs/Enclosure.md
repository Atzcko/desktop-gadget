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

- **BOOT must be pressable at plug-in time** — it is the flash rescue path,
  and the only recovery if an OTA image crash-loops
  ([[T4-S3#The BOOT button (GPIO0)]], [[D034 - Updates ship over the air]]).
- The 2×15 header carries the [[D035 - The Lab may only touch pins the firmware does not own|Lab whitelist pins]];
  an enclosure that blocks it turns the bench-tool app into decoration.
- USB-C is the only power. No battery logic exists in firmware.

## Related

- [[T4-S3]] — the board the case must fit
- [[index]]
