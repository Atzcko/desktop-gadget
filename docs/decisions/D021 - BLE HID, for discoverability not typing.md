---
title: D021 - BLE HID, for discoverability not typing
type: decision
id: D021
date: 2026-08-16
status: accepted
supersedes: the brief's deferral of the HID phase
origin: owner
tags:
  - decision
  - ble
---

# D021 — BLE HID, for discoverability rather than typing

**Ask.** "I want to be able to see and connect to the device via Bluetooth settings."

## Why the previous answer was "you can't"

macOS **System Settings ▸ Bluetooth only lists devices implementing a profile it knows how to pair with** — classic Bluetooth, or BLE profiles like HID and audio. A custom GATT peripheral is invisible there *by design*, regardless of signal strength, advertising interval, name placement or appearance value. Fixing the earlier advertising bug ([[D017 - BLE and Wi-Fi coexistence]]) made the device show up correctly in BLE *scanners*, but scanners are not System Settings.

There is exactly one way to be in that list: **be a HID device.**

## Decision

The device now advertises a **BLE HID keyboard** service alongside the existing NUS emotion service.

```
Primary advertisement   flags + appearance 0x03C1 (Keyboard)
                        + 16-bit service 0x1812 (HID) + name
Scan response           128-bit NUS service UUID
```

The appearance value **and** the 16-bit HID service UUID are both required — macOS uses them together to classify the device as a pairable input device.

> [!important] It is a keyboard that never types
> No key reports are ever sent. The HID service exists purely as the ticket into the Bluetooth settings list. The emotion API still travels over NUS, exactly as before. This is a deliberate half-step into the HID phase the brief deferred: the plumbing is now in place if real key/media reports are wanted later.

## Advertising budget

The 31-byte limit is why the payload had to be reorganised:

| Field | Bytes |
|---|---|
| Flags | 3 |
| Appearance | 4 |
| 16-bit service list (HID) | 4 |
| Name `"Flip Clock"` | 12 |
| **Total** | **23** |

The 128-bit NUS UUID is 18 bytes on its own and cannot share the primary packet — it lives in the scan response.

## Pairing

HID characteristics must be encrypted, so pairing is **mandatory**, not optional:

```cpp
NimBLEDevice::setSecurityAuth(/*bond=*/true, /*mitm=*/false, /*sc=*/true);
NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
```

**Just Works**, no passkey. MITM protection would require displaying a 6-digit code mid-pairing, which means hijacking the clock face, and it protects nothing on a device that transmits no keystrokes.

> [!warning] macOS may open Keyboard Setup Assistant
> On first pair, macOS sees an unknown keyboard and may open the assistant asking you to press keys near Shift. Close it — the device cannot answer, and the pairing completes regardless. This is the one unavoidable wart of presenting as a keyboard.

## Escape hatches

Both live in **Settings ▸ BLE**:

- **Pairable** switch — off returns the device to GATT-only (appearance Generic Clock, no HID service). Invisible to Bluetooth settings, still reachable from `tools/flipclock.py` and any BLE scanner.
- **Clear pairings** — `NimBLEDevice::deleteAllBonds()`. Bonding state lives on *both* ends, so a half-forgotten pairing is a classic wedge: the Mac remembers a bond the device has dropped and refuses to reconnect. Clear here **and** "Forget This Device" on the Mac.

**Cost:** +3 KB flash, negligible RAM.
