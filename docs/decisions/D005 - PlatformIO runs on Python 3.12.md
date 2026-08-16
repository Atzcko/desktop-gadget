---
title: D005 - PlatformIO runs on Python 3.12
type: decision
id: D005
date: 2026-08-16
status: accepted
origin: environment constraint
tags:
  - decision
  - toolchain
---

# D005 — PlatformIO runs on Python 3.12, in its own venv

**Problem.** This Mac's default `python3` is **3.14.6** (Homebrew). PlatformIO Core 6.x is not validated against 3.14, and several of its pinned dependencies predate it.

**Decision.** Install PlatformIO Core into a dedicated virtualenv built on Homebrew's `python3.12`:

```bash
/opt/homebrew/bin/python3.12 -m venv ~/.platformio-venv
~/.platformio-venv/bin/pip install platformio
```

Result: **PlatformIO Core 6.1.19**. Every build prepends `~/.platformio-venv/bin` to `PATH`.

**Why a venv rather than `pip install --user`.** Keeps the ESP32 toolchain manager isolated from the user's system Python, and makes the whole install disposable — `rm -rf ~/.platformio-venv` and reinstall if it ever gets wedged.

**Consequence.** Any shell that runs `pio` needs:

```bash
export PATH="$HOME/.platformio-venv/bin:$PATH"
```

This is documented in [[CLAUDE]] so it is not rediscovered later.
