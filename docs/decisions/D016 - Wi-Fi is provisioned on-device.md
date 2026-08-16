---
title: D016 - Wi-Fi is provisioned on-device
type: decision
id: D016
date: 2026-08-16
status: accepted
origin: fell out of D013
tags:
  - decision
  - security
  - settings
---

# D016 — Wi-Fi is provisioned on-device

**Decision.** `DEFAULT_WIFI_SSID` and `DEFAULT_WIFI_PASSWORD` in `config.h` are **empty**. The network is chosen from a live scan on the Settings screen and the password typed on the on-screen keyboard, then stored in NVS.

**Why this is better than a credential in a header.**

1. **The password never leaves the owner's hands.** It is not typed into a file, not pasted into a chat, not read by an assistant, and not sitting in a working tree where a stray `git add -f` could commit it.
2. **`config.h` stops being sensitive.** It is still gitignored, but it now contains nothing worse than a latitude.
3. **Changing networks does not need a toolchain.** Moving the clock to a different Wi-Fi is a thirty-second interaction with the device, not a rebuild and reflash.

Filling the defaults in still works as a first-boot fallback for anyone who would rather not type on a 600×450 panel.

## Follow-up — "Save & close" did not save Wi-Fi

Originally the credentials were committed **only** by the Connect button.
Picking a network, typing the password and then tapping **Save & close** — the
obvious thing to do, and the thing every other setting on that screen responds
to — silently discarded both.

`close_cb()` now commits the picked SSID and any typed password alongside
everything else, and calls `net_apply_wifi()` when either changed.

> [!tip] A "Save" button must save everything on the screen
> Any field that a Save button does not persist is a trap, no matter how
> reasonable the split looked while writing it. Connect remains as the
> "apply it right now" shortcut; it is no longer the only path.

**Implementation notes.**

- `net_scan()` de-duplicates SSIDs, because mesh networks advertise the same name from several APs and an undeduplicated list is confusing.
- Hidden networks (empty SSID) are filtered out — they cannot be joined from a scan list anyway.
- The password field uses `lv_textarea_set_password_mode(true)`, so the characters are not shoulder-surfable on a desk device.
- The scan is **blocking** (~2 s). It is only ever called from the LVGL task while the Settings screen is open, where a brief pause is expected — never from the clock path.
