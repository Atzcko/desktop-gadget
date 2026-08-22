---
title: Scripting
type: stage
status: done
date: 2026-08-22
tags:
  - stage
---

# Stage — scripted apps

Apps as Lua files, added and removed on a running clock.
Design: [[D037 - Apps become Lua scripts]].

## Scope

1. Lua 5.4.7 vendored in `lib/lua/`, PSRAM allocator, instruction budget.
2. LittleFS on the 3.5 MB partition; scripts at `/apps/<name>.lua`.
3. Dynamic registry — `app_count()`/`app_at()`, native first.
4. `GET|POST|DELETE /apps` and `tools/app`.
5. Bindings: `ui`, `gpio`, `millis`, `log`, `back`.

## The script contract

```lua
function on_create() end   -- required; build the screen
function on_tick()   end   -- optional; every LVGL loop
function on_back()   end   -- optional; return true to consume the gesture
function on_exit()   end   -- optional; last call before the state closes
```

## Acceptance

- [x] Upload appears in `/apps` without a reboot (2026-08-22)
- [x] Delete removes it live; re-add works
- [x] A syntax error is rejected **at upload** with the parser's message,
      leaving any previous version of that name intact
- [x] `?name=../../etc/passwd` rejected
- [x] Device healthy throughout — uptime never reset across the whole cycle
- [ ] **Owner's bench:** the Blink and Uptime tiles appear in the drawer and run
- [ ] **Owner's bench:** Blink drives GPIO21 (LED or meter)
- [ ] **Owner's bench:** a runaway script (`while true do end`) is stopped by the
      budget and reports on screen rather than freezing the clock

## Known gaps

I²C and UART are not exposed to scripts yet — the Lab covers them natively.
No auth, consistent with `/update`.
