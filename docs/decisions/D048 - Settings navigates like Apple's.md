---
title: D048 - Settings navigates like Apple's
type: decision
status: accepted
date: 2026-08-28
tags:
  - decision
  - ui
---

# D048 — Settings navigates like Apple's

## Context

Settings used an `lv_tabview`: six tab buttons across the top, pages swiped
horizontally. The owner asked for vertical everything — tabs up/down, content
up/down — "copy what Apple has in macOS and iOS settings layout."

Apple ships two layouts for the same content, chosen by shape. So does this
device now.

## Decision

**Landscape (600×450) = macOS System Settings**: a 190 px sidebar on the
left — six rows, icon + name, selected row highlighted — and the section's
page in the pane beside it. Both scroll vertically; nothing scrolls
horizontally anywhere.

**Portrait (450×600) = iOS Settings**: the section list full-width, each row
with a chevron; tapping pushes the page full-screen with its title. Back pops
page → list before anything else.

The tabview is gone entirely. Section pages are the same six flex columns
they always were — the content code did not change, only the container
plumbing around it. The back stack gained one level, and the chip and the
edge swipe walk it identically: **editor → page (portrait) → unsaved-changes
guard → host**. Fixed 560-px children (the Wi-Fi and city lists) went
percentage-width — they had already been silently clipping in portrait.

## Consequences

- No horizontal gesture remains in Settings, so nothing competes with the
  left-edge back swipe (D029) — a conflict the tabview's swipe always had.
- The sidebar highlight is a landscape-only affordance; iOS rows do not show
  selection because the list is never visible at the same time as a page.
- Six sections is comfortable in a 190 px sidebar; a seventh fits without
  redesign (the sidebar scrolls).

## Related

- [[D045 - One chrome, every shape]] — the shape-switching doctrine
- [[D029 - Back is a system gesture, not a widget event]] · [[D019 - Text entry gets its own screen]]
