---
title: D015 - Abu Dhabi locale
type: decision
id: D015
date: 2026-08-16
status: accepted
origin: owner
tags:
  - decision
  - time
---

# D015 — Abu Dhabi, and why the TZ string looks odd

The owner is in **Abu Dhabi**. Defaults set accordingly:

| Setting | Value |
|---|---|
| City | Abu Dhabi |
| Latitude | 24.4539 |
| Longitude | 54.3773 |
| POSIX TZ | `<+04>-4` |

## Verifying the timezone rather than guessing it

Checked against the system tzdata rather than assumed:

```
2026-01-15  utcoffset=4:00:00  dst=0:00:00  abbrev=+04
2026-07-15  utcoffset=4:00:00  dst=0:00:00  abbrev=+04
```

**No DST in either direction**, so the string carries no changeover rules — unusual, and the reason it is so short compared to the European and US entries in the timezone table.

## Two things that look like typos and are not

> [!warning] `<+04>` — the angle brackets are required
> POSIX TZ wants a *name* for the zone abbreviation. Since 2017 the IANA database stopped inventing pseudo-abbreviations like "GST" and emits numeric ones instead. A bare `+04` is not parseable as a name, so POSIX requires it wrapped in angle brackets. `tail -c 60 /usr/share/zoneinfo/Asia/Dubai` emits exactly `<+04>-4` — this string is copied from tzdata, not composed by hand.

> [!warning] The offset sign is inverted
> `-4` means **UTC+4**. POSIX defines the field as *the value added to local time to reach UTC*, which is the opposite sign from how people write offsets. Getting this backwards puts the clock 8 hours out, and it is the single most common mistake with POSIX TZ strings.

**Consequence.** All DST handling is delegated to libc via `setenv("TZ", ...)` + `tzset()`. This firmware never computes a daylight-saving transition — which also means the European and US entries in the timezone table are correct for free.
