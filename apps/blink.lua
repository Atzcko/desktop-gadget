-- blink.lua — drive a header pin from a script.
--
-- Proves the whole point of the runtime: this file was uploaded to a running
-- clock over Wi-Fi, appeared in the drawer, and drives real hardware — with no
-- rebuild and no reboot.

local PIN     = 21          -- any Lab whitelist pin
local PERIOD  = 500         -- ms per half cycle

local running = false
local level   = false
local next_ms = 0
local lbl, btn

function on_create()
  ui.label{ text = "Blink", size = 32, x = 24, y = 18 }
  ui.label{ text = "GPIO" .. PIN .. ", " .. PERIOD .. " ms",
            size = 20, color = 0x8A8A8A, x = 24, y = 62 }

  ui.card{ x = 24, y = 104, w = 250, h = 150 }
  lbl = ui.label{ text = "LOW", font = "mid", color = 0x8A8A8A, x = 24, y = 150, w = 250 }

  btn = ui.button{ text = "Start", x = 24,  y = 280, w = 250, h = 70, size = 24,
                   on_click = function()
                     running = not running
                     ui.set_text(btn, running and "Stop" or "Start")
                     if not running then
                       gpio.write(PIN, false)
                       level = false
                       paint()
                     end
                   end }

  ui.button{ text = "Back", x = 320, y = 280, w = 200, h = 70, size = 24,
             on_click = function() back() end }

  gpio.mode(PIN, "out")
  gpio.write(PIN, false)
  paint()
end

function paint()
  ui.set_text(lbl, level and "HIGH" or "LOW")
  ui.set_color(lbl, level and 0x2FBF71 or 0x8A8A8A)
end

function on_tick()
  if not running then return end
  local now = millis()
  if now < next_ms then return end
  next_ms = now + PERIOD
  level = not level
  gpio.write(PIN, level)
  paint()
end

-- Called on the way out. The runtime returns every pin to Hi-Z anyway; this
-- is here to show the hook exists.
function on_exit()
  log("blink stopped")
end
