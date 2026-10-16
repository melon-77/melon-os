-- Desktop test harness for melon-run.lua: a stand-in for the TI-Nspire Lua API (Lua 5.1, like the calculator).
--   lua5.1 tools/sim.lua bot [first] [last]   a bot follows each level's path; every level must be finished
--   lua5.1 tools/sim.lua frames OUTDIR        writes the draw calls of a few scenes for tools/render.py
--   lua5.1 tools/sim.lua stress               random key mashing in every state; must not raise errors
local mode, a1, a2 = ...
local here = arg[0]:match("^(.*)/tools/") or "."

NOW = 0
local STORE = {}
local rec = nil

local function noop() end
local function mkgc()
  local gc = {}
  local function log(...) if rec then rec[#rec + 1] = table.concat({...}, "\t") end end
  function gc:setColorRGB(r, g, b) log("color", r, g, b) end
  function gc:fillRect(x, y, w, h) log("rect", x, y, w, h) end
  function gc:drawRect(x, y, w, h) log("drect", x, y, w, h) end
  function gc:fillArc(x, y, w, h, s, e) log("arc", x, y, w, h) end
  function gc:drawLine(x1, y1, x2, y2) log("line", x1, y1, x2, y2) end
  function gc:fillPolygon(t)
    assert(type(t) == "table" and #t >= 6 and #t % 2 == 0, "bad polygon")
    for i = 1, #t do assert(t[i] == t[i], "NaN in polygon") end
    log("poly", unpack(t))
  end
  local size = 9
  function gc:setFont(f, s, sz) size = sz end
  function gc:getStringWidth(s) return #s * size * 0.62 end
  function gc:drawString(s, x, y, v) log("text", x, y, size, s) end
  gc.setPen = noop
  return gc
end

platform = {window = {invalidate = noop, width = function() return 318 end, height = function() return 212 end}}
timer = {start = noop, stop = noop, getMilliSecCounter = function() return math.floor(NOW * 1000) end}
var = {store = function(k, v) STORE[k] = v end, recall = function(k) return STORE[k] end}
document = {markChanged = noop}
on = {}

dofile(here .. "/melon-run.lua")
local R = MELONRUN
local G = R.G
on.construction()
on.resize(318, 212)
local gc = mkgc()

local DT = 1 / 30
local function tick()
  NOW = NOW + DT
  on.timer()
  on.paint(gc)
end

local function lanesOf() return G.lvl.lanes end

-- the bot: steer toward the generated path a little ahead, jump when the next row is empty
local function botStep()
  local p, lv = G.pl, G.lvl
  if G.state ~= "play" or p.lost then return end
  local r = math.floor(p.z)
  local look = math.min(lv.len - 1, r + 1)
  local target = lv.path[look] or p.lane
  if lv.chasm[look] then target = p.lane end
  if target > p.lane then on.arrowKey("right") elseif target < p.lane then on.arrowKey("left") end
  local k = math.floor(p.pp + 0.5)
  local nextT = R.tileAt(r + 1, k)
  if p.ground and nextT == 0 and p.z - r > 0.45 then on.arrowKey("up") end
end

if mode == "bot" then
  local first, last = tonumber(a1) or 1, tonumber(a2) or R.LEVELS
  local failed = 0
  for n = first, last do
    G.unlocked = R.LEVELS
    local ok = false
    G.state = "menu"; G.menuSel = 1; G.startLevel = n
    on.enterKey()
    local lvl = G.lvl
    local steps = 0
    while steps < 30 * 60 do
      botStep()
      tick()
      steps = steps + 1
      if G.state ~= "play" then break end
      if G.lvl ~= lvl then ok = true break end
    end
    if G.state == "won" then ok = true end
    local lv = R.levelParams(n)
    print(string.format("level %2d  %d sides x %d  len %3d  speed %.1f  %s%s", n, lv.sides, lv.cols, lv.len, lv.speed,
      ok and "finished" or ("FAILED at z=" .. string.format("%.1f", G.pl.z) .. " state=" .. G.state),
      ok and (" in " .. string.format("%.1f", steps * DT) .. " s") or ""))
    if not ok then failed = failed + 1 end
  end
  print(failed == 0 and "all levels finished" or (failed .. " level(s) failed"))
  os.exit(failed == 0 and 0 or 1)
elseif mode == "frames" then
  local out = a1 or "frames"
  local function dump(name)
    rec = {}
    on.paint(gc)
    local f = assert(io.open(out .. "/" .. name .. ".txt", "w"))
    f:write(table.concat(rec, "\n"), "\n")
    f:close()
    rec = nil
  end
  for _ = 1, 20 do tick() end
  dump("01-menu")
  G.unlocked = R.LEVELS
  G.startLevel = 1
  on.enterKey()
  for _ = 1, 30 do botStep(); tick() end
  dump("02-level1")
  -- step onto the right wall and keep going
  for _ = 1, 3 do on.arrowKey("right"); tick() end
  dump("03-turning")
  for _ = 1, 15 do tick() end
  dump("04-on-wall")
  on.arrowKey("up")
  for _ = 1, 6 do tick() end
  dump("05-jump")
  G.state = "menu"; G.startLevel = 12; G.menuSel = 1
  on.enterKey()
  for _ = 1, 150 do botStep(); tick() end
  dump("06-level12")
  G.state = "menu"; G.startLevel = 26
  on.enterKey()
  for _ = 1, 200 do botStep(); tick() end
  dump("07-level26")
  -- fall off
  for _ = 1, 400 do
    tick()
    if G.state == "dead" then break end
  end
  for _ = 1, 3 do tick() end
  dump("08-dead")
  G.state = "menu"; G.menuSel = 3
  on.enterKey()
  tick()
  dump("09-help")
  print("frames written to " .. out)
elseif mode == "stress" then
  math.randomseed(1)
  local keys = {"left", "right", "up", "down"}
  for i = 1, 40000 do
    local x = math.random(1, 20)
    if x <= 4 then on.arrowKey(keys[x])
    elseif x == 5 then on.enterKey()
    elseif x == 6 and math.random() < 0.05 then on.escapeKey()
    elseif x == 7 then on.charIn(tostring(math.random(0, 9)))
    end
    tick()
  end
  print("stress ok, state=" .. G.state .. " unlocked=" .. G.unlocked .. " best=" .. G.best)
else
  print("usage: lua5.1 tools/sim.lua bot|frames DIR|stress")
  os.exit(2)
end
