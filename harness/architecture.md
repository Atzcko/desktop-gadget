---
title: Architecture
type: architecture
project: Desktop gadget
created: 2026-09-13
updated: 2026-09-13
---

# Architecture — Desktop gadget

One screen. Detail lives in the instance notes under `agents/`.

## Roster

| Agent | Tier | Model | Library | Purpose |
|---|---|---|---|---|

## Routing

| A request that looks like | Goes to |
|---|---|

## Escalation

Two failures on one task return `ESCALATE:` to the conductor. Three
escalations from one agent across tasks are a roster problem and go in
[[evolution]].

## Spawn allowlists

Leads spawn specialists, specialists spawn workers, workers spawn nothing.
Exceptions and why:

## Background agents

| Agent | Cadence | Last run |
|---|---|---|
| gardener | once per session | |
| librarian | once per session | |

## Budgets

From the brief. Token or cost ceilings, quiet hours, anything the conductor
must respect when delegating.

## Related

- [[H001 - Initial roster]]
