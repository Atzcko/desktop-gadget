---
title: D017 - BLE and Wi-Fi coexistence
type: decision
id: D017
date: 2026-08-16
status: accepted
supersedes: the brief's "v1 is Wi-Fi only, do NOT enable BLE"
origin: owner request
tags:
  - decision
  - ble
  - wifi
---

# D017 — BLE and Wi-Fi coexistence

**The owner asked for BLE**, reversing the brief's "v1 is Wi-Fi only. Do NOT enable BLE." Recorded as a deliberate change of direction, not drift.

**Library: NimBLE, not Bluedroid.** Roughly a third of the flash and RAM for a peripheral-only role. Pinned `h2zero/NimBLE-Arduino @ ^1.4.2` — NimBLE-Arduino 2.x requires ESP32 core 3.x and we are on 2.0.17 via `espressif32@6.12.0`.

**Cost, measured:** enabling BLE + the async HTTP server took the image from 22.8 % → 26.9 % flash and 15.5 % → 18.1 % RAM. Comfortable.

## Two crashes, both boot loops, both worth remembering

The ESP32-S3 has **one** 2.4 GHz radio. Wi-Fi and BLE share it, and the coexistence layer is unforgiving.

### 1. Ordering — `coex_core_enable` abort

```
abort() at coex_core_enable <- coex_enable <- esp_bt_controller_enable
                            <- NimBLEDevice::init <- ble_begin
```

`NimBLEDevice::init()` brings up radio coexistence. If Wi-Fi has **already** claimed the radio, that abort fires and the device boot-loops forever.

> [!warning] BLE must start before Wi-Fi
> `ble_begin()` is called **before** `net_begin()` in `setup()`. Swapping them back reproduces the boot loop instantly. This is the single most fragile ordering in the firmware.

### 2. Modem sleep — the error message that names itself

```
E wifi: Error! Should enable WiFi modem sleep when both WiFi and
        Bluetooth are enabled!!!!!!
```

`net_begin()` had `WiFi.setSleep(false)`, chosen deliberately for lower HTTP latency. That is **illegal once BT is enabled**: modem sleep is the mechanism by which Wi-Fi yields airtime to Bluetooth. Without it the Wi-Fi task aborts.

Now `WiFi.setSleep(ble_is_running())`, and toggling BLE from Settings adjusts it at runtime — `ble_begin()` sets sleep on, `ble_stop()` sets it off again.

**Accepted cost.** Modem sleep adds tens of milliseconds of latency to HTTP requests. For a desk clock receiving an occasional emotion POST, that is invisible.

## Diagnosis note

Both crashes were solved by **decoding the backtrace**, not guessing:

```bash
xtensa-esp32s3-elf-addr2line -pfiaC -e ~/.pio-builds/desktop-gadget/t4s3/firmware.elf 0x420b77ce ...
```

The env already sets `monitor_filters = esp32_exception_decoder`, which does this automatically in `pio device monitor`. Reach for it first on any abort.
