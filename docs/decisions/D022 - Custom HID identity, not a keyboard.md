---
title: D022 - Custom HID identity, not a keyboard
type: decision
id: D022
date: 2026-08-16
status: accepted
supersedes: the keyboard descriptor in D021
origin: owner
tags:
  - decision
  - ble
  - hid
---

# D022 — Custom HID identity, not a keyboard

**Ask.** "Make it a custom HID device, so my Mac knows it's not a keyboard, it's a desktop gadget."

[[D021 - BLE HID, for discoverability not typing]] got the device into System Settings ▸ Bluetooth by presenting a **keyboard**. That worked, but it lies about what the device is, and it is why macOS opens Keyboard Setup Assistant.

## What actually determines the identity

Three things, and all three had to change together:

| Layer | Was | Now |
|---|---|---|
| HID report descriptor | Generic Desktop / Keyboard | **Vendor page `0xFF00`, Usage `0x01`** |
| GAP appearance | `0x03C1` Keyboard | **`0x03C0` Generic HID** |
| DIS model string | *(absent)* | **"Flip Clock Desktop Gadget"** |

The **report descriptor is the one that matters**. Appearance only picks an icon; the descriptor is what the OS parses to decide *what kind of device this is*. A vendor-defined usage page has no OS-level meaning, so macOS enumerates a generic HID device and **cannot** mistake it for a text-input device.

> [!success] Keyboard Setup Assistant is gone
> That dialog fires because macOS believes an unknown *keyboard* has arrived. Remove the keyboard usage and the trigger disappears — this was a side benefit worth having on its own.

## The descriptor is real, not decorative

Two 32-byte reports are declared, and the **output report (host → device) is wired to the emotion engine**:

```
[0] 0xE0 magic   [1] state 1..6   [2..3] duration_s LE   [4..] message
```

Same validation and the same queue as BLE NUS and HTTP — see [[D018 - Emotion API - one engine, two transports]]. So this is a genuine gadget protocol rather than a descriptor that exists only to satisfy a pairing dialog.

> [!note] Not practically drivable from macOS userspace
> Once macOS claims a HID device, writing output reports to it needs IOHIDManager with privileges that ordinary scripts do not have. The protocol is implemented and correct, but **NUS and HTTP remain the transports to actually use** from `tools/flipclock.py`. It is there for completeness and for hosts that are less restrictive.

## PnP identity

```cpp
hid->pnp(0x02, 0x303A, 0x4001, 0x0100);   /* USB-IF source, Espressif VID */
```

`0x303A` is Espressif's real USB vendor ID and this genuinely is an Espressif part, so this is not VID squatting. The product ID is ours.

## The fallback switch

`Settings ▸ BLE ▸ Identify as keyboard (fallback)`, default **off**.

macOS lists BLE devices it recognises as HID. A keyboard is certain to be listed; a purely vendor-defined descriptor is *less* certain — behaviour here is not contractual, and it varies by OS version. Rather than gamble on one identity, both are shipped and one switch chooses.

> [!warning] Hosts cache the HID descriptor per bond
> Changing this switch on a device that is already paired **will not** change what the Mac thinks it is. The descriptor is read once, at pairing, and cached against the bond. To actually switch identity:
>
> 1. macOS: System Settings ▸ Bluetooth ▸ ⓘ ▸ **Forget This Device**
> 2. Device: Settings ▸ BLE ▸ **Clear pairings**
> 3. Pair again
>
> Skipping this is the single most likely reason the change appears to have done nothing.
