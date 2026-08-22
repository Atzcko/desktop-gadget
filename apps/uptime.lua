-- uptime.lua — how long since the last boot, in the clock's own type.
--
-- No hardware, no state to clean up. About as small as a useful app gets.

local d_h, d_m, d_s

local function card(x, y)
  ui.card{ x = x, y = y, w = 178, h = 190, radius = 22 }
  return ui.label{ text = "00", font = "digits", x = x, y = y + 18, w = 178 }
end

function on_create()
  d_h = card(14,  40)
  d_m = card(210, 40)
  d_s = card(406, 40)

  ui.label{ text = "hours              minutes            seconds",
            size = 18, color = 0x8A8A8A, x = 30, y = 244 }

  ui.button{ text = "Back", x = 14, y = 300, w = 200, h = 66, size = 24,
             on_click = function() back() end }
end

function on_tick()
  local t = millis() // 1000
  ui.set_text(d_h, string.format("%02d", t // 3600))
  ui.set_text(d_m, string.format("%02d", (t // 60) % 60))
  ui.set_text(d_s, string.format("%02d", t % 60))
end
