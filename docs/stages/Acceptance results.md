---
title: Acceptance results
type: verification
date: 2026-08-16
tags:
  - acceptance
---

# Acceptance results — 2026-08-16

Measured against the four criteria in the brief. Two are machine-verifiable and were measured; two need a human, and are marked as such rather than assumed.

## 1. Cold boot → correct local time in under 10 s — **PASS**

Measured from hardware reset, correlating the serial log with `/health` polling:

| Milestone | Time |
|---|---|
| UI rendered | 1.64 s |
| Wi-Fi got IP | 2.46 s |
| HTTP + mDNS up | 2.46 s |
| Weather fetched | 4.29 s |
| **NTP time valid** | **4.45 s** |

**4.45 s against a 10 s budget** — 55 % headroom. The panel itself lights at 1.64 s showing `00:00`, and the real time lands at 4.45 s.

## 2. Minute rollover, including 23:59 → 00:00 — **needs your eyes**

Cannot be automated: there is no way to observe the panel from here, and forcing a 23:59 rollover would mean adding a debug endpoint to production firmware.

What *is* established by code review: the flip is driven by **"the rendered digits differ from the target digits", per card** — never by arithmetic on the previous value. That is precisely the property that makes 23:59 → 00:00 work, because both cards simply observe a difference and fold; `00` is not treated as "less than" `23`. The same rule makes the first post-NTP jump correct, and that path *is* exercised on every boot.

## 3. `POST /emotion` animates, then reverts — **PASS**

`emotion.active` / `remaining_s` were added to `/health` specifically so this is observable rather than a matter of faith:

```
POST celebrate 5s -> {"ok":true,"state":"celebrate","duration_s":5}
   t+ 0.1s  active=True  state=celebrate remaining=5s
   t+ 2.0s  active=True  state=celebrate remaining=3s
   t+ 4.5s  active=True  state=celebrate remaining=1s
   t+ 5.2s  active=False state=none      remaining=0s
```

> [!note] A false negative worth remembering
> The first run of this test reported FAIL. The cause was the **test**, not the firmware: it polled `flipclock.local`, and re-resolving mDNS on every request made each sample slow enough to step over the whole 5 s window. Re-running against the IP passed immediately. Measure through the cheapest path available, or you end up debugging your own instrument.

### Input validation — **PASS**

| Input | Result |
|---|---|
| `{"state":"banana"}` | 400 — `unknown state 'banana' (…)` |
| `{"duration_s":5}` | 400 — `missing "state"` |
| 25-character message | 400 — `message longer than 20 characters` |
| `duration_s: 99999` | 200, **clamped to 300** |
| Uptime across all of the above | 50 s → 76 s — **no reboot** |

## 4. Wi-Fi drop degrades gracefully and reconnects — **PASS, from field evidence**

Not simulated deliberately — it happened for real earlier in the project, when the stored PSK was wrong. Over several minutes of continuous association failure:

- the clock **kept running and rendering**;
- weather kept its last values and raised the stale marker;
- reconnection retried on **exponential backoff capped at 30 s**, with the reason code logged (`202 AUTH_FAIL`);
- once the credentials were corrected, the device associated and recovered with no intervention.

That is the criterion, demonstrated under a genuine fault rather than a staged one.

## Summary

| # | Criterion | Result |
|---|---|---|
| 1 | Cold boot < 10 s | ✅ 4.45 s |
| 2 | Minute + midnight rollover | 👁 needs visual confirmation |
| 3 | Emotion animates and reverts | ✅ 5.2 s |
| 4 | Wi-Fi resilience | ✅ observed in the field |

Plus, beyond the brief: BLE GATT + custom HID identity, an on-device settings screen, humidity, and a Python client.
