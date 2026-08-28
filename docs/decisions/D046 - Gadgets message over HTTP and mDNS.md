---
title: D046 - Gadgets message over HTTP and mDNS
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - api
  - apps
---

# D046 — Gadgets message over HTTP and mDNS

## Context

The owner wants devices like this one messaging each other on the LAN, each
with an identifier, suggesting a small background HTTP server — and asked for
the options to be researched.

## The research, honestly sized

| Transport | Verdict |
|---|---|
| **HTTP into the existing async server + mDNS-SD discovery** | ✅ The server, mDNS, and HTTPClient are already running or linked. Delivery is push (the receiver's server takes the POST). Identity is a TXT record. Works across APs on one LAN. |
| ESP-NOW | ❌ Identity is a MAC address; channel-locked to the AP's channel; no reach across APs; a second radio protocol to coexist with. |
| MQTT | ❌ Needs a broker — an external dependency the scope boundary (no cloud beyond Open-Meteo/NTP) exists to forbid. |
| Raw UDP | ❌ Reinvents discovery and delivery confirmation that mDNS + HTTP give for free. |

## Decision

- **Identity = the device name** (Settings ▸ BLE ▸ name). One name for BLE
  and messaging. Two clocks on one LAN must be named apart — the name IS the
  address label.
- Every device advertises **`_gadget-msg._tcp`** with `name=<identity>` in
  TXT. Peers are found with one mDNS browse; no pairing, no registry.
- **`POST /msg {"from","text"}`** delivers; the arrival is announced on the
  line display through the emotion queue (the one legal door, D018), and a
  16-deep inbox feeds the Messages app. `GET /messages` reads it;
  `POST /send {"ip","text"}` makes this device send — which is also the test
  seam: curl can play a second gadget in both directions.
- **Scans and sends block for seconds**, so they run on a worker task with a
  command queue; the UI requests and polls. The web task only ever calls
  `msg_store()`, which locks against the readers.

## Limits, stated

- Inbox is RAM: a reboot clears it. Deliberate — messages between desk
  gadgets are ephemera, not mail.
- No delivery to a device that is off; no relay, no store-and-forward.
- No auth, same posture as the whole HTTP API (D034): private LAN.
- One mDNS hostname (`flipclock.local`) — the SERVICE browse dedupes by name,
  but a second device should also get a distinct hostname when one arrives;
  noted, not built.

## Related

- [[D039 - The device must be drivable without a finger]] — /send and
  /messages double as the remote test rig
- [[D018 - Emotion API - one engine, two transports]] · [[D045 - One chrome, every shape]]
