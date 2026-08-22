---
title: OTA
type: stage
status: done
date: 2026-08-22
tags:
  - stage
---

# Stage — OTA

Ship firmware over Wi-Fi; retire the cable to rescue duty.
Decision and design: [[D034 - Updates ship over the air]].

## Scope

1. `POST /update` — raw image → inactive OTA slot (already in the partition
   table since Stage 0; checked, not assumed).
2. `tools/ota` — build, discover, push, and refuse success until `/health`
   reports the new version.
3. The device narrates its own update through the emotion queue.

## Acceptance

- [x] v1.16.1 delivered over the air — `1.16.0 e78bab6` → `1.16.1 307ff46`,
      confirmed by `/health` with fresh uptime (2026-08-22)
- [x] Settings and Wi-Fi survive the update (NVS untouched)
- [x] `tools/ota` fails loudly when the version did not change
- [x] Wrong file rejected by the magic-byte check in the first chunk
- [x] v1.17.0 (the Lab) shipped entirely OTA — the mechanism in daily use

## Residual risk, accepted

No rollback (stock Arduino core): an image that boots then crash-loops needs
the cable. Mitigations: local build must succeed before push; BOOT must stay
reachable in any [[Enclosure]].
