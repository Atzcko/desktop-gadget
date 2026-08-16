---
title: D006 - Keep build artifacts out of iCloud
type: decision
id: D006
date: 2026-08-16
status: accepted
origin: environment constraint
tags:
  - decision
  - build
---

# D006 — Keep build artifacts out of iCloud

**Problem.** The project lives at `~/Library/Mobile Documents/com~apple~CloudDocs/Projects/Desktop gadget` — inside **iCloud Drive**. A PlatformIO build tree (`.pio/`) is tens of thousands of small object files, rewritten on every compile.

That is a bad combination in three separate ways:

1. **Sync storm.** Every rebuild queues thousands of file changes to iCloud.
2. **Eviction.** macOS can evict infrequently-used iCloud files to the cloud and replace them with placeholders. A dataless `.o` or toolchain file surfaces as a bizarre mid-build failure.
3. **Speed.** The iCloud file provider adds real latency to the many-small-files access pattern a compiler generates.

**Decision.**

- The **source** stays in iCloud — it is small, text, and worth syncing.
- The **build tree** is redirected out of it:

```ini
[platformio]
build_dir = ~/.pio-builds/desktop-gadget
```

- The **library clone** lives at `~/.local/src/LilyGo-AMOLED-Series`, also outside iCloud.
- `.gitignore` covers `.pio/` regardless, as a belt-and-braces measure.

**Consequence.** A clean checkout on another machine rebuilds from scratch, which is correct behaviour anyway. Nothing in the build tree is precious.
