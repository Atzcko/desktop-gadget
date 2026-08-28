-- portrait_ok — blink.lua: square-wave any output-capable header pin.
--
-- The first line's portrait_ok tag tells the host this script lays itself
-- out for both shapes (D045); SCREEN_W/SCREEN_H are the live size.

local PINS   = { 21, 38, 39, 40, 41, 42, 47, 48, 43, 44, 1, 2, 3, 4 }
local PERIOD = 500

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
  local W, H = SCREEN_W, SCREEN_H
  local portrait = H > W

  ui.label{ text = "Blink", size = 32, x = 24, y = 16 }
  ui.label{ text = PERIOD .. " ms half cycle", size = 18, color = 0x8A8A8A,
            x = 24, y = 56 }

  -- pin selector: < [GPIO n] > ; state card beside it in landscape,
  -- below it in portrait.
  local sel_y = 96
  local mid_w = W - 24*2 - 76*2 - 16     -- selector fills the row
  if not portrait then mid_w = 168 end

  ui.button{ text = "<", x = 24, y = sel_y, w = 76, h = 92, size = 28,
             on_click = function() select_pin(-1) end }
  ui.card{ x = 108, y = sel_y, w = mid_w, h = 92 }
  lbl_pin = ui.label{ text = "GPIO", size = 28, x = 108, y = sel_y + 30, w = mid_w }
  ui.button{ text = ">", x = 108 + mid_w + 8, y = sel_y, w = 76, h = 92, size = 28,
             on_click = function() select_pin(1) end }

  local st_x, st_y, st_w = 386, sel_y, 190
  if portrait then st_x, st_y, st_w = 24, 208, W - 48 end
  ui.card{ x = st_x, y = st_y, w = st_w, h = 92 }
  lbl_state = ui.label{ text = "LOW", font = "mid", color = 0x8A8A8A,
                        x = st_x, y = st_y + 22, w = st_w }

  -- uniform strip (D045): back bottom-left is the host's chip in native
  -- apps; scripts draw their own in the same place.
  local strip_y = H - 56 - 10
  ui.button{ text = "Back", x = 12, y = strip_y, w = 132, h = 56, size = 20,
             on_click = function() back() end }
  btn_run = ui.button{ text = "Start", x = 156, y = strip_y, w = W - 156 - 12,
                       h = 56, size = 24,
                       on_click = function()
                         running = not running
                         if not running then
                           level = false
                           gpio.write(pin(), false)
                         end
                         next_ms = millis()
                         paint()
                       end }

  ui.label{ text = "pick a pin, then Start", size = 18, color = 0x6E6E6E,
            x = 24, y = strip_y - 34 }

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
  running = false
  gpio.write(pin(), false)
  log("blink stopped on GPIO " .. pin())
end
