---
title: D047 - Messages are conversations, and the clock wears the badge
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - apps
  - ui
---

# D047 — Messages are conversations, and the clock wears the badge

## Context

v1.24.0's Messages was a flat inbox with a peer dropdown — functional, and
nothing like how anyone reads messages. The owner asked for the phone shape:
WhatsApp/iMessage — address someone by name or number, type on a keyboard,
and see a small icon on the home screen when something arrives.

## Decision

**Conversations, not an inbox.** History entries carry a peer and a
direction; the app is two views — a chats list (peer, preview, age) and a
thread of bubbles, theirs left in charcoal, ours right in dark green, capped
at 3/4 screen width. In-app navigation stacks under the host's back:
composer → thread → list → the host pops the app. The same `back()` hook
that saves Settings' passwords drives all of it.

**Addressing: a name, or literally a number.** The recipient field takes a
contact name or a dotted-quad IP. The contact book learns name→address three
ways, which is the load-bearing design:

1. an mDNS scan merges every advertising gadget,
2. sending to a typed name/IP remembers it,
3. **receiving remembers the sender against the connection's source IP** —
   the address arrives with the message, so replying never needs a scan.

**The badge.** A pill with an envelope, top-left of the clock (the battery's
opposite corner), hidden at zero, hidden in line mode under the same
discipline as the battery chip — and **tappable**: it opens Messages through
`app_host_request_open()`, the same door the HTTP API uses. Unread clears on
opening the app, and stays cleared while you are looking at it.

## Consequences

- Sent bubbles appear instantly (`msg_note_sent` writes history before the
  worker delivers); a failed delivery reports under the thread rather than
  un-sending the bubble.
- History is 32 entries RAM, per D046's ephemera stance — a reboot clears
  conversations. The moment that stops feeling right, LittleFS is sitting
  there.
- A typed name that no scan or message has ever resolved cannot be sent to —
  the app says so and suggests Scan or an IP. Names are labels; the IP is
  the address (D046).

## Related

- [[D046 - Gadgets message over HTTP and mDNS]] — the transport under this
- [[D029 - Back is a system gesture, not a widget event]] · [[D019 - Text entry gets its own screen]]
