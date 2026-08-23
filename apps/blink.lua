-- blink.lua — square-wave any output-capable header pin.
--
-- The pin is chosen on the device, so this is a bench tool rather than a demo:
-- pick a pin, start it, put a meter or an LED on it.

-- Every SAFE_PIN the runtime will let a script drive. GPIO0 is deliberately
-- absent: the BOOT button hard-wires it to ground, so driving it high is one
-- press away from shorting the pad (D036).
local PINS   = { 21, 38, 39, 40, 41, 42, 47, 48, 43, 44, 1, 2, 3, 4 }
local PERIOD = 500          -- ms per half cycle

local idx     = 1
local running = false
local level   = false
local next_ms = 0
local lbl_pin, lbl_state, btn_run

local function pin() return PINS[idx] end

local function paint()
  ui.set_text(lbl_pin, "GPIO " .. pin())
  ui.set_text(lbl_state, level and "HIGH" or "LOW")
  ui.set_color(lbl_state, level and 0x2FBF71 or 0x8A8A8A)
  ui.set_text(btn_run, running and "Stop" or "Start")
end

-- Leaving a pin behind still driving is the one thing a bench tool must never
-- do, so every pin change releases the old one first.
local function select_pin(step)
  if running then return end
  gpio.write(pin(), false)
  gpio.mode(pin(), "in")
  idx = ((idx - 1 + step) % #PINS) + 1
  level = false
  gpio.mode(pin(), "out")
  gpio.write(pin(), false)
  paint()
end

function on_create()
  ui.label{ text = "Blink", size = 32, x = 24, y = 16 }
  ui.label{ text = PERIOD .. " ms half cycle", size = 18, color = 0x8A8A8A,
            x = 24, y = 56 }

  ui.button{ text = "<", x = 24, y = 96, w = 76, h = 92, size = 28,
             on_click = function() select_pin(-1) end }
  ui.card{ x = 108, y = 96, w = 168, h = 92 }
  lbl_pin = ui.label{ text = "GPIO", size = 28, x = 108, y = 126, w = 168 }
  ui.button{ text = ">", x = 284, y = 96, w = 76, h = 92, size = 28,
             on_click = function() select_pin(1) end }

  ui.card{ x = 386, y = 96, w = 190, h = 92 }
  lbl_state = ui.label{ text = "LOW", font = "mid", color = 0x8A8A8A,
                        x = 386, y = 118, w = 190 }

  btn_run = ui.button{ text = "Start", x = 24, y = 220, w = 336, h = 84, size = 26,
                       on_click = function()
                         running = not running
                         if not running then
                           level = false
                           gpio.write(pin(), false)
                         end
                         next_ms = millis()
                         paint()
                       end }

  ui.button{ text = "Back", x = 386, y = 220, w = 190, h = 84, size = 26,
             on_click = function() back() end }

  ui.label{ text = "pick a pin with the arrows, then Start",
            size = 18, color = 0x6E6E6E, x = 24, y = 330 }

  gpio.mode(pin(), "out")
  gpio.write(pin(), false)
  paint()
end

function on_tick()
  if not running then return end
  local now = millis()
  if now < next_ms then return end
  next_ms = now + PERIOD
  level = not level
  gpio.write(pin(), level)
  paint()
end

function on_exit()
  -- The runtime returns every pin to Hi-Z on the way out; this makes the
  -- intent explicit and stops the square wave immediately.
  running = false
  gpio.write(pin(), false)
  log("blink stopped on GPIO " .. pin())
end
