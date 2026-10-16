platform.apiLevel = '2.0'
-- melon run: a space-tunnel runner for the TI-Nspire CX II (and CX, OS 3.6 or later).
-- A fan game inspired by the gameplay of Run 3 (Player 03); its code, levels, runner and art are melon's own,
-- nothing is taken from the original game.
-- Run down a tunnel floating in space, step onto the walls to turn the whole tunnel,
-- jump over the gaps. Controls: left/right (4/6) move a lane, up/enter/8/space jumps,
-- down (2) drops faster, esc pauses.

local floor, sqrt, sin, cos, pi, abs = math.floor, math.sqrt, math.sin, math.cos, math.pi, math.abs
local min, max = math.min, math.max

-- Tunables (units: one lane is 1 wide, one row is 1 long)
local JUMP_V = 6.2        -- take-off speed
local GRAV = 20           -- gravity
local FAST_GRAV = 42      -- gravity while holding down
local LANE_SPEED = 9      -- lanes per second when side-stepping
local EYE = 0.95          -- camera height above the floor
local CAM_BACK = 2.4      -- camera distance behind the runner
local NEAR = 0.6          -- near clipping distance
local DRAW = 18           -- rows drawn ahead
local LEVELS = 30

local W, H = 318, 212
local CX, CY, F = 159, 89, 190

-- Game state, one table so the desktop test harness can read it
local G = {
  state = "menu", menuSel = 1, startLevel = 1, unlocked = 1, best = 0,
  mode = "levels", levelNo = 1, chunk = 0, dist = 0, cells = 0, runCells = 0,
  banner = nil, bannerT = 0, deadT = 0, showFps = false, fps = 0,
  cam = {x = 0, y = 0, angle = 0},
  pl = {},
}

---------------------------------------------------------------------------
-- deterministic random numbers (Park-Miller), so a level is the same every time
local rngState = 1
local function seed(s)
  rngState = floor(s) % 2147483646 + 1
end
local function rnd()
  rngState = (rngState * 16807) % 2147483647
  return rngState / 2147483647
end
local function rndInt(a, b) return a + floor(rnd() * (b - a + 1)) end

---------------------------------------------------------------------------
-- levels
local THEMES = {
  {name = "Grey Line",   tile = {125, 140, 168}, crumble = {150, 104, 70}},
  {name = "Green Belt",  tile = {84, 168, 112},  crumble = {160, 110, 60}},
  {name = "Violet Run",  tile = {146, 112, 200}, crumble = {170, 100, 80}},
  {name = "Amber Drift", tile = {200, 156, 78},  crumble = {120, 80, 60}},
  {name = "Ice Duct",    tile = {92, 172, 196},  crumble = {165, 105, 75}},
  {name = "Rose Shaft",  tile = {196, 104, 128}, crumble = {130, 90, 60}},
}

local function levelParams(n)
  local S, c
  if n <= 4 then S, c = 4, 3
  elseif n <= 9 then S, c = 5, 3
  elseif n <= 15 then S, c = 6, 2
  elseif n <= 22 then S, c = 7, 2
  else S, c = 8, 2 end
  return {
    n = n, sides = S, cols = c, lanes = S * c,
    len = min(80 + 6 * n, 220),
    speed = min(6 + 0.2 * (n - 1), 11.5),
    density = max(0.3, 0.88 - 0.028 * n),
    crumble = n >= 3 and min(0.4, 0.03 * n) or 0,
    theme = THEMES[(n - 1) % #THEMES + 1],
  }
end

-- geometry: a regular polygon, side 0 at the bottom, lanes running left to right
local function buildGeometry(lv)
  local S, c, L = lv.sides, lv.cols, lv.lanes
  local d = 2 * pi / S
  local sideLen = c
  local apothem = sideLen / (2 * math.tan(pi / S))
  local R = apothem / cos(pi / S)
  local VX, VY = {}, {}
  for i = 0, S do
    local a = -pi / 2 + i * d - pi / S
    VX[i], VY[i] = R * cos(a), R * sin(a)
  end
  local PX, PY = {}, {}           -- perimeter points between lanes
  for k = 0, L - 1 do
    local s = floor(k / c)
    local f = (k - s * c) / c
    PX[k] = VX[s] + (VX[s + 1] - VX[s]) * f
    PY[k] = VY[s] + (VY[s + 1] - VY[s]) * f
  end
  local NX, NY = {}, {}           -- outward normal of each side
  for s = 0, S - 1 do
    local a = -pi / 2 + s * d
    NX[s], NY[s] = cos(a), sin(a)
  end
  lv.delta, lv.apothem = d, apothem
  lv.VX, lv.VY, lv.PX, lv.PY, lv.NX, lv.NY = VX, VY, PX, PY, NX, NY
end

-- tiles: 0 = none, 1 = tile, 2 = cracked (breaks if you land on it)
local function buildLevel(lv, seedValue)
  seed(seedValue)
  buildGeometry(lv)
  local L, len, S, c, n = lv.lanes, lv.len, lv.sides, lv.cols, lv.n
  local rows, chasm = {}, {}
  for r = 0, len - 1 do
    local row = {}
    for k = 0, L - 1 do row[k] = 1 end
    rows[r] = row
  end
  local airRows = lv.speed * (2 * JUMP_V / GRAV)
  local maxGap = max(1, floor(airRows * 0.7))
  lv.airRows = airRows

  -- pattern segments
  local kinds = {"random", "random", "chasm"}
  if n >= 2 then kinds[#kinds + 1] = "sides" end
  if n >= 3 then kinds[#kinds + 1] = "crumble" end
  if n >= 4 then kinds[#kinds + 1] = "lanes"; kinds[#kinds + 1] = "sides" end
  if n >= 5 then kinds[#kinds + 1] = "holes"; kinds[#kinds + 1] = "chasm" end
  if n >= 7 then kinds[#kinds + 1] = "checker" end
  if n >= 10 then kinds[#kinds + 1] = "spiral" end

  local r = 8
  while r < len - 8 do
    local seg = rndInt(6, 14)
    if r + seg > len - 6 then seg = len - 6 - r end
    local kind = kinds[rndInt(1, #kinds)]
    if kind == "chasm" then
      local g = rndInt(1, maxGap)
      for i = r, r + g - 1 do
        for k = 0, L - 1 do rows[i][k] = 0 end
        chasm[i] = true
      end
      -- room to land: a jump from the last tile before the chasm must come down before anything else
      seg = g + floor(airRows) + rndInt(2, 4)
    else
      local phase = rndInt(0, 1)
      local cleared = {}
      if kind == "sides" then
        for _ = 1, rndInt(1, max(1, S - 2)) do cleared[rndInt(0, S - 1)] = true end
      end
      local twist = rndInt(0, L - 1)
      for i = r, r + seg - 1 do
        for k = 0, L - 1 do
          local t = 1
          local s = floor(k / c)
          if kind == "random" then
            if rnd() > lv.density + 0.1 then t = 0 end
          elseif kind == "holes" then
            if rnd() > lv.density * 0.7 then t = 0 end
          elseif kind == "sides" then
            if cleared[s] then t = 0 end
          elseif kind == "lanes" then
            if (k + phase) % 2 == 1 then t = 0 end
          elseif kind == "checker" then
            if (k + i + phase) % 2 == 1 then t = 0 end
          elseif kind == "crumble" then
            t = rnd() < 0.85 and 2 or 0
          elseif kind == "spiral" then
            if ((k - floor((i - r) / 2) + twist) % L) >= c + 1 then t = 0 end
          end
          if t == 1 and rnd() < lv.crumble then t = 2 end
          rows[i][k] = t
        end
      end
    end
    r = r + seg
  end

  -- a guaranteed way through: a path that wanders across lanes and walls
  local path = {}
  local p = floor((c - 1) / 2)
  lv.startLane = p
  local cool = 0
  local safeAfter = -1
  for i = 0, len - 1 do
    if chasm[i] then
      path[i] = p
      safeAfter = i + floor(airRows) + 2
    else
      local row = rows[i]
      if i >= 8 and cool <= 0 and not chasm[i + 1] and not chasm[i - 1] and rnd() < 0.24 then
        local np = p + (rnd() < 0.5 and -1 or 1)
        if row[p % L] == 0 then row[p % L] = 1 end
        p = np
        cool = 2
      end
      if row[p % L] == 0 or (i <= safeAfter and row[p % L] == 2) then row[p % L] = 1 end
      if chasm[i + 1] and row[p % L] == 2 then row[p % L] = 1 end
      cool = cool - 1
      path[i] = p
    end
  end
  for i = len - 6, len - 1 do
    for k = 0, L - 1 do rows[i][k] = 1 end
  end

  -- power cells along the path
  local cells = {}
  local i = 12
  while i < len - 4 do
    if not chasm[i] and rnd() < 0.55 then
      cells[#cells + 1] = {r = i, lane = path[i], got = false}
    end
    i = i + rndInt(6, 11)
  end

  lv.rows, lv.path, lv.chasm, lv.cells = rows, path, chasm, cells
  return lv
end

---------------------------------------------------------------------------
-- saving (kept in the document's variables: save the document to keep them)
local function loadSave()
  local ok, v = pcall(function() return var.recall("mrunlock") end)
  if ok and type(v) == "number" then G.unlocked = max(1, min(LEVELS, floor(v))) end
  ok, v = pcall(function() return var.recall("mrunbest") end)
  if ok and type(v) == "number" then G.best = floor(v) end
  ok, v = pcall(function() return var.recall("mruncells") end)
  if ok and type(v) == "number" then G.cells = floor(v) end
  G.startLevel = G.unlocked
end

local function save()
  pcall(function() var.store("mrunlock", G.unlocked) end)
  pcall(function() var.store("mrunbest", G.best) end)
  pcall(function() var.store("mruncells", G.cells) end)
  pcall(function() document.markChanged() end)
end

---------------------------------------------------------------------------
-- runner
local function resetPlayer(lv)
  local p = G.pl
  p.z = 0.2
  p.lane = lv.startLane
  p.pp = lv.startLane
  p.h, p.vh = 0, 0
  p.ground, p.lost, p.fast = true, false, false
  p.coyote, p.phase = 0, 0
  G.cam.angle = 0
  G.cam.x, G.cam.y = 0, -lv.apothem + EYE
end

local function startLevel(n, keepPlayer)
  local lv = levelParams(n)
  local seedValue = n * 7919 + 13
  if G.mode == "infinite" then
    G.chunk = G.chunk + 1
    lv = levelParams(min(LEVELS, 1 + G.chunk * 2))
    lv.len = 140
    seedValue = (timer.getMilliSecCounter() + G.chunk * 104729) % 2000000000
  end
  G.lvl = buildLevel(lv, seedValue)
  resetPlayer(G.lvl)
  if G.mode == "levels" then
    G.banner = "LEVEL " .. n
    G.sub = lv.theme.name .. "  -  " .. lv.sides .. " sides"
  else
    G.banner = G.chunk == 1 and "INFINITE" or nil
    G.sub = G.chunk == 1 and "how far can you go?" or nil
  end
  G.bannerT = 2
end

local function newGame(mode, n)
  G.mode = mode
  G.chunk = 0
  G.dist = 0
  G.runCells = 0
  G.levelNo = n or 1
  G.state = "play"
  startLevel(G.levelNo)
end

local function tileAt(z, k)
  local lv = G.lvl
  local r = floor(z)
  if r < 0 or r >= lv.len then return 1 end
  return lv.rows[r][k % lv.lanes]
end

local function laneNear() return floor(G.pl.pp + 0.5) end

local function die()
  G.state = "dead"
  G.deadT = 0
  if G.mode == "infinite" then
    local d = floor(G.dist)
    if d > G.best then G.best = d; G.newBest = true else G.newBest = false end
  end
  save()
end

local function levelDone()
  if G.mode == "levels" then
    if G.levelNo >= LEVELS then
      G.unlocked = LEVELS
      G.state = "won"
      save()
      return
    end
    G.levelNo = G.levelNo + 1
    if G.levelNo > G.unlocked then G.unlocked = G.levelNo end
    save()
  end
  local lane = G.pl.lane
  startLevel(G.levelNo)
  -- keep the runner's place across the lanes when the tunnel has the same shape
  G.pl.lane, G.pl.pp = lane % G.lvl.lanes, lane % G.lvl.lanes
  if G.pl.lane >= G.lvl.cols then G.pl.lane, G.pl.pp = G.lvl.startLane, G.lvl.startLane end
end

local function jump()
  local p = G.pl
  if p.lost then return end
  if p.ground or p.coyote > 0 then
    p.ground = false
    p.coyote = 0
    p.vh = JUMP_V
  end
end

local function step(dir)
  local p = G.pl
  if p.lost then return end
  if abs(p.lane + dir - p.pp) <= 1.5 then p.lane = p.lane + dir end
end

local function updatePlay(dt)
  local lv, p = G.lvl, G.pl
  p.z = p.z + lv.speed * dt
  G.dist = G.dist + lv.speed * dt
  p.phase = p.phase + dt * lv.speed * 1.6

  -- side-step
  local diff = p.lane - p.pp
  local s = LANE_SPEED * dt
  if abs(diff) <= s then p.pp = p.lane else p.pp = p.pp + (diff > 0 and s or -s) end

  local k = laneNear()
  if p.ground then
    if tileAt(p.z, k) == 0 then
      p.coyote = p.coyote + dt
      if p.coyote > 0.05 then p.ground = false; p.vh = 0; p.coyote = 0 end
    else
      p.coyote = 0
    end
  else
    p.vh = p.vh - (p.fast and FAST_GRAV or GRAV) * dt
    p.h = p.h + p.vh * dt
    if p.h <= 0 and not p.lost then
      local t = tileAt(p.z, k)
      if t > 0 and p.h > -0.12 then
        if t == 2 and p.vh < -2 then
          lv.rows[floor(p.z)][k % lv.lanes] = 0   -- cracked tile gives way
          p.lost = true
        else
          p.h, p.vh, p.ground, p.fast = 0, 0, true, false
        end
      elseif p.h <= -0.12 then
        p.lost = true
      end
    end
    if p.lost and p.h < -3.5 then die() return end
  end

  -- power cells
  for i = 1, #lv.cells do
    local cell = lv.cells[i]
    if not cell.got and abs(p.z - (cell.r + 0.5)) < 0.55 and (cell.lane - k) % lv.lanes == 0 and p.h < 0.9 and not p.lost then
      cell.got = true
      G.cells = G.cells + 1
      G.runCells = G.runCells + 1
    end
  end

  if p.z >= lv.len then levelDone() end
end

local function updateCamera(dt)
  local lv, p, cam = G.lvl, G.pl, G.cam
  local sideU = floor((p.pp + 0.5) / lv.cols)
  local target = -sideU * lv.delta
  cam.angle = cam.angle + (target - cam.angle) * min(1, dt * 9)
  if p.lost then return end
  local k = laneNear() % lv.lanes
  local side = floor(k / lv.cols)
  local gx, gy = G.groundPoint(p.pp)
  local tx = (gx - lv.NX[side] * EYE) * 0.85
  local ty = (gy - lv.NY[side] * EYE) * 0.85
  local a = min(1, dt * 12)
  cam.x = cam.x + (tx - cam.x) * a
  cam.y = cam.y + (ty - cam.y) * a
end

-- point on the floor under the middle of lane position pp (unwrapped)
function G.groundPoint(pp)
  local lv = G.lvl
  local q = (pp + 0.5) % lv.lanes
  local s = floor(q / lv.cols)
  local f = (q - s * lv.cols) / lv.cols
  return lv.VX[s] + (lv.VX[s + 1] - lv.VX[s]) * f, lv.VY[s] + (lv.VY[s + 1] - lv.VY[s]) * f
end

---------------------------------------------------------------------------
-- menu demo: the first tunnel flies by behind the title
local demo = {z = 0, angle = 0}

local function demoLevel()
  if not G.demoLvl then
    G.demoLvl = buildLevel(levelParams(4), 4242)
  end
  return G.demoLvl
end

---------------------------------------------------------------------------
-- update
local lastMs = nil
local frames, fpsT = 0, 0

local function update(dt)
  if G.bannerT > 0 then G.bannerT = G.bannerT - dt end
  if G.state == "play" then
    local t = dt
    while t > 0 do
      local h = min(t, 0.02)
      updatePlay(h)
      if G.state ~= "play" then break end
      t = t - h
    end
    if G.lvl then updateCamera(dt) end
  elseif G.state == "dead" then
    G.deadT = G.deadT + dt
  elseif G.state == "menu" or G.state == "help" then
    local lv = demoLevel()
    demo.z = demo.z + dt * 5
    if demo.z > lv.len - DRAW - 4 then demo.z = 0 end
    demo.angle = demo.angle + dt * 0.35
  end
end

---------------------------------------------------------------------------
-- drawing
local stars = {}
do
  seed(77)
  for i = 1, 60 do
    stars[i] = {a = rnd() * 2 * pi, r = 20 + rnd() * 220, b = 90 + floor(rnd() * 165)}
  end
end

local U, V = {}, {}

local function drawStars(gc, angle)
  for i = 1, #stars do
    local s = stars[i]
    local a = s.a + angle
    local x, y = CX + s.r * cos(a), CY - s.r * sin(a) * 0.8
    if x >= 0 and x < W and y >= 0 and y < H then
      gc:setColorRGB(s.b, s.b, s.b)
      gc:fillRect(x, y, s.b > 220 and 2 or 1, 1)
    end
  end
end

local function shade(c, f)
  return floor(c[1] * f), floor(c[2] * f), floor(c[3] * f)
end

local quad = {0, 0, 0, 0, 0, 0, 0, 0}

local function drawTunnel(gc, lv, camX, camY, camZ, angle, dim)
  local L, cols = lv.lanes, lv.cols
  local ca, sa = cos(angle), sin(angle)
  local PX, PY = lv.PX, lv.PY
  for i = 0, L - 1 do
    local dx, dy = PX[i] - camX, PY[i] - camY
    U[i] = dx * ca - dy * sa
    V[i] = dx * sa + dy * ca
  end
  local zNear = camZ + NEAR
  local r0 = floor(zNear)
  local rEnd = min(r0 + DRAW, lv.len)
  local rows = lv.rows
  local tile, crumble = lv.theme.tile, lv.theme.crumble

  -- light at the end of the tunnel
  if lv.len <= r0 + DRAW + 6 then
    local inv = F / (lv.len - camZ)
    local poly = {}
    for i = 0, L - 1 do
      poly[#poly + 1] = CX + U[i] * inv
      poly[#poly + 1] = CY - V[i] * inv
    end
    local g = floor(255 * dim)
    gc:setColorRGB(g, g, floor(g * 0.85))
    gc:fillPolygon(poly)
  end

  -- rows from far to near, two rows per band (merged when both are the same tile)
  local bFirst = floor(r0 / 2)
  local bLast = floor((rEnd - 1) / 2)
  for b = bLast, bFirst, -1 do
    local ra, rb = 2 * b, 2 * b + 1
    local rowA = (ra >= r0 and ra < rEnd) and rows[ra] or nil
    local rowB = (rb >= r0 and rb < rEnd) and rows[rb] or nil
    local zMid = rb - camZ
    local fog = 1.1 - zMid / (DRAW + 1)
    if fog > 1 then fog = 1 elseif fog < 0.06 then fog = 0.06 end
    fog = fog * dim
    if b % 2 == 1 then fog = fog * 0.84 end
    for k = 0, L - 1 do
      local ta = rowA and rowA[k] or 0
      local tb = rowB and rowB[k] or 0
      local i1 = (k + 1) % L
      local lf = fog
      if (k % cols) % 2 == 1 then lf = lf * 0.84 end
      if floor(k / cols) % 2 == 1 then lf = lf * 0.94 end
      for part = 1, 2 do
        local t, za, zb
        if part == 1 then
          if ta > 0 and ta == tb then t, za, zb = ta, ra, rb + 1
          else t, za, zb = tb, rb, rb + 1 end
        else
          if ta > 0 and ta ~= tb then t, za, zb = ta, ra, ra + 1 else t = 0 end
        end
        if t > 0 then
          if za < zNear then za = zNear end
          if za < zb then
            local ia, ib = F / (za - camZ), F / (zb - camZ)
            if t == 2 then gc:setColorRGB(shade(crumble, lf)) else gc:setColorRGB(shade(tile, lf)) end
            quad[1], quad[2] = CX + U[k] * ia, CY - V[k] * ia
            quad[3], quad[4] = CX + U[i1] * ia, CY - V[i1] * ia
            quad[5], quad[6] = CX + U[i1] * ib, CY - V[i1] * ib
            quad[7], quad[8] = CX + U[k] * ib, CY - V[k] * ib
            gc:fillPolygon(quad)
          end
        end
      end
    end
  end
  return ca, sa
end

local function project(x, y, z, camX, camY, camZ, ca, sa)
  local dx, dy = x - camX, y - camY
  local u, v = dx * ca - dy * sa, dx * sa + dy * ca
  local inv = F / (z - camZ)
  return CX + u * inv, CY - v * inv, inv
end

local function drawCells(gc, lv, camX, camY, camZ, ca, sa)
  local cells = lv.cells
  for i = #cells, 1, -1 do
    local c = cells[i]
    local dz = c.r + 0.5 - camZ
    if not c.got and dz > NEAR + 0.3 and dz < DRAW then
      local k = c.lane % lv.lanes
      local s = floor(k / lv.cols)
      local gx, gy = G.groundPoint(c.lane)
      local x, y, inv = project(gx - lv.NX[s] * 0.4, gy - lv.NY[s] * 0.4, c.r + 0.5, camX, camY, camZ, ca, sa)
      local r = 0.17 * inv
      local fog = min(1, 1.25 - dz / DRAW)
      gc:setColorRGB(floor(255 * fog), floor(214 * fog), floor(40 * fog))
      gc:fillPolygon({x, y - r * 1.4, x + r, y, x, y + r * 1.4, x - r, y})
    end
  end
end

local function drawRunner(gc, lv, camX, camY, camZ, ca, sa)
  local p = G.pl
  local k = laneNear() % lv.lanes
  local s = floor(k / lv.cols)
  local nx, ny = lv.NX[s], lv.NY[s]
  local gx, gy = G.groundPoint(p.pp)
  -- shadow while in the air above a tile
  if p.h > 0.05 and tileAt(p.z, k) > 0 then
    local sx, sy, inv = project(gx, gy, p.z, camX, camY, camZ, ca, sa)
    local w = 0.34 * inv
    gc:setColorRGB(20, 20, 30)
    gc:fillArc(sx - w / 2, sy - w / 6, w, w / 3, 0, 360)
  end
  local fx, fy, inv = project(gx - nx * p.h, gy - ny * p.h, p.z, camX, camY, camZ, ca, sa)
  local hx, hy = project(gx - nx * (p.h + 0.56), gy - ny * (p.h + 0.56), p.z, camX, camY, camZ, ca, sa)
  local u = inv / 135                      -- size unit
  local function at(t) return fx + (hx - fx) * t, fy + (hy - fy) * t end
  -- melon's runner: a round striped melon on two legs
  local swing = p.ground and sin(p.phase) * 5 * u or 2 * u
  local lx, ly = at(0.3)
  gc:setColorRGB(48, 92, 52)
  gc:fillRect(lx - 7 * u + swing, ly, 4 * u, fy - ly + 1)
  gc:fillRect(lx + 3 * u - swing, ly, 4 * u, fy - ly + 1)
  gc:setColorRGB(232, 92, 92)                      -- red shoes
  gc:fillRect(lx - 8 * u + swing, fy - 3 * u, 6 * u, 3 * u)
  gc:fillRect(lx + 2 * u - swing, fy - 3 * u, 6 * u, 3 * u)
  local mx, my = at(0.68)
  local r = 17 * u
  gc:setColorRGB(96, 178, 84)
  gc:fillArc(mx - r, my - r, 2 * r, 2 * r, 0, 360)
  gc:setColorRGB(52, 122, 56)                      -- rind stripes
  gc:fillArc(mx - r * 0.62, my - r, r * 0.34, 2 * r, 0, 360)
  gc:fillArc(mx + r * 0.28, my - r, r * 0.34, 2 * r, 0, 360)
  gc:fillArc(mx - r * 0.12, my - r, r * 0.24, 2 * r, 0, 360)
  gc:setColorRGB(250, 250, 240)                    -- eyes
  gc:fillArc(mx - 8 * u, my - 6 * u, 7 * u, 8 * u, 0, 360)
  gc:fillArc(mx + 1 * u, my - 6 * u, 7 * u, 8 * u, 0, 360)
  gc:setColorRGB(20, 24, 20)
  gc:fillArc(mx - 6 * u, my - 4 * u, 4 * u, 5 * u, 0, 360)
  gc:fillArc(mx + 3 * u, my - 4 * u, 4 * u, 5 * u, 0, 360)
  local sx, sy = at(1.08)                          -- stalk
  gc:setColorRGB(120, 90, 50)
  gc:drawLine(mx, my - r, sx + 3 * u, sy)
end

local function text(gc, s, x, y, size, r, g, b, centred)
  gc:setFont("sansserif", "b", size)
  if centred then x = x - gc:getStringWidth(s) / 2 end
  gc:setColorRGB(0, 0, 0)
  gc:drawString(s, x + 1, y + 1, "top")
  gc:setColorRGB(r, g, b)
  gc:drawString(s, x, y, "top")
end

local function drawHud(gc)
  local lv = G.lvl
  local prog = min(1, G.pl.z / lv.len)
  gc:setColorRGB(40, 40, 55)
  gc:fillRect(60, 4, W - 120, 5)
  gc:setColorRGB(120, 230, 140)
  gc:fillRect(60, 4, floor((W - 120) * prog), 5)
  if G.mode == "levels" then
    text(gc, "Lv " .. G.levelNo, 4, 0, 9, 230, 230, 240)
  else
    text(gc, floor(G.dist) .. " m", 4, 0, 9, 230, 230, 240)
  end
  local cs = "* " .. G.runCells
  gc:setFont("sansserif", "b", 9)
  text(gc, cs, W - 6 - gc:getStringWidth(cs), 0, 9, 255, 214, 40)
  if G.showFps then text(gc, G.fps .. " fps", 4, H - 16, 7, 160, 160, 170) end
  if G.bannerT > 0 and G.banner then
    text(gc, G.banner, CX, 40, 16, 255, 255, 255, true)
    if G.sub then text(gc, G.sub, CX, 64, 9, 200, 200, 215, true) end
  end
end

local function panel(gc, x, y, w, h)
  gc:setColorRGB(14, 16, 28)
  gc:fillRect(x, y, w, h)
  gc:setColorRGB(120, 230, 140)
  gc:drawRect(x, y, w, h)
end

local MENU = {"Levels", "Infinite", "How to play"}

local function drawMenu(gc)
  local lv = demoLevel()
  drawTunnel(gc, lv, 0, 0, demo.z, demo.angle, 0.55)
  text(gc, "MELON RUN", CX, 16, 24, 255, 255, 255, true)
  text(gc, "a space tunnel runner for TI-Nspire CX II", CX, 50, 9, 120, 230, 140, true)
  panel(gc, CX - 80, 74, 160, 92)
  for i = 1, #MENU do
    local label = MENU[i]
    if i == 1 then label = "Levels   < " .. G.startLevel .. " >" end
    local y = 80 + (i - 1) * 26
    if i == G.menuSel then
      gc:setColorRGB(60, 120, 80)
      gc:fillRect(CX - 76, y - 1, 152, 22)
    end
    text(gc, label, CX, y + 2, 11, 240, 240, 250, true)
  end
  local info = "Best " .. G.best .. " m   Cells " .. G.cells .. "   Unlocked " .. G.unlocked .. "/" .. LEVELS
  text(gc, info, CX, 176, 7, 180, 180, 195, true)
  text(gc, "arrows + enter", CX, 192, 7, 140, 140, 155, true)
end

local HELP = {
  "Run through the tunnel. Don't fall into space.",
  "left / right (4 / 6): move one lane",
  "Step past the edge of the floor onto a wall:",
  "the whole tunnel turns and the wall is the floor.",
  "up / enter / 8 / space: jump",
  "down (2): drop faster   esc: pause",
  "Brown tiles are cracked: they break if you land",
  "on them, but you can run across. Grab the cells.",
}

local function drawHelp(gc)
  drawTunnel(gc, demoLevel(), 0, 0, demo.z, demo.angle, 0.35)
  panel(gc, 8, 8, W - 16, H - 16)
  text(gc, "How to play", CX, 14, 12, 120, 230, 140, true)
  for i = 1, #HELP do text(gc, HELP[i], 16, 36 + (i - 1) * 19, 9, 230, 230, 240) end
  text(gc, "esc: back", CX, H - 26, 7, 160, 160, 175, true)
end

local function drawGame(gc)
  local lv, p, cam = G.lvl, G.pl, G.cam
  local camZ = p.z - CAM_BACK
  drawStars(gc, cam.angle)
  local ca, sa = drawTunnel(gc, lv, cam.x, cam.y, camZ, cam.angle, 1)
  drawCells(gc, lv, cam.x, cam.y, camZ, ca, sa)
  drawRunner(gc, lv, cam.x, cam.y, camZ, ca, sa)
  drawHud(gc)
end

local function drawOverlay(gc, title, lines)
  panel(gc, CX - 110, 60, 220, 30 + 16 * #lines)
  text(gc, title, CX, 66, 12, 255, 255, 255, true)
  for i = 1, #lines do text(gc, lines[i], CX, 72 + 16 * i, 9, 210, 210, 225, true) end
end

---------------------------------------------------------------------------
-- events
function on.construction()
  loadSave()
  timer.start(0.01)
end

function on.resize(w, h)
  W, H = w, h
  CX, CY = floor(w / 2), floor(h * 0.42)
  F = h * 0.9
end

function on.activate() timer.start(0.01) end
function on.deactivate() timer.stop() end

function on.timer()
  local now = timer.getMilliSecCounter()
  local dt = lastMs and (now - lastMs) / 1000 or 0.03
  lastMs = now
  if dt > 0.1 then dt = 0.1 elseif dt < 0 then dt = 0.03 end
  frames, fpsT = frames + 1, fpsT + dt
  if fpsT >= 1 then G.fps = frames; frames, fpsT = 0, 0 end
  if G.state ~= "pause" then update(dt) end
  platform.window:invalidate()
end

function on.paint(gc)
  gc:setColorRGB(4, 4, 12)
  gc:fillRect(0, 0, W, H)
  gc:setPen("thin", "smooth")
  local st = G.state
  if st == "menu" then drawMenu(gc)
  elseif st == "help" then drawHelp(gc)
  else
    drawGame(gc)
    if st == "pause" then
      drawOverlay(gc, "Paused", {"enter: keep running", "esc: back to the menu"})
    elseif st == "dead" then
      if G.mode == "levels" then
        drawOverlay(gc, "Lost in space", {"Level " .. G.levelNo .. ", " .. floor(min(1, G.pl.z / G.lvl.len) * 100) .. "% of the way",
          "enter: try again   esc: menu"})
      else
        drawOverlay(gc, G.newBest and "New best!" or "Lost in space", {floor(G.dist) .. " m, " .. G.runCells .. " cells (best " .. G.best .. " m)",
          "enter: run again   esc: menu"})
      end
    elseif st == "won" then
      drawOverlay(gc, "You made it!", {"All " .. LEVELS .. " tunnels run.", "Try Infinite next.", "enter: menu"})
    end
  end
end

local function press(key)
  local st = G.state
  if st == "menu" then
    if key == "up" then G.menuSel = (G.menuSel - 2) % #MENU + 1
    elseif key == "down" then G.menuSel = G.menuSel % #MENU + 1
    elseif G.menuSel == 1 and key == "left" then G.startLevel = max(1, G.startLevel - 1)
    elseif G.menuSel == 1 and key == "right" then G.startLevel = min(G.unlocked, G.startLevel + 1)
    elseif key == "enter" then
      if G.menuSel == 1 then newGame("levels", G.startLevel)
      elseif G.menuSel == 2 then newGame("infinite", 1)
      else G.state = "help" end
    end
  elseif st == "help" then
    if key == "esc" or key == "enter" then G.state = "menu" end
  elseif st == "play" then
    if key == "left" then step(-1)
    elseif key == "right" then step(1)
    elseif key == "up" or key == "enter" then jump()
    elseif key == "down" then if not G.pl.ground then G.pl.fast = true end
    elseif key == "esc" then G.state = "pause" end
  elseif st == "pause" then
    if key == "enter" then G.state = "play"; lastMs = nil
    elseif key == "esc" then G.state = "menu"; G.startLevel = min(G.unlocked, max(G.startLevel, G.levelNo)) end
  elseif st == "dead" then
    if key == "enter" and G.deadT > 0.4 then
      if G.mode == "levels" then newGame("levels", G.levelNo) else newGame("infinite", 1) end
    elseif key == "esc" then G.state = "menu"; G.startLevel = G.unlocked end
  elseif st == "won" then
    if key == "enter" or key == "esc" then G.state = "menu" end
  end
  platform.window:invalidate()
end

function on.arrowKey(key) press(key) end
function on.enterKey() press("enter") end
function on.escapeKey() press("esc") end
function on.charIn(ch)
  if ch == "4" then press("left")
  elseif ch == "6" then press("right")
  elseif ch == "8" or ch == " " then press("up")
  elseif ch == "2" then press("down")
  elseif ch == "5" then press("enter")
  elseif ch == "f" then G.showFps = not G.showFps
  end
end

-- read by the desktop test harness (tools/sim.lua); harmless on the calculator
MELONRUN = {G = G, press = press, update = update, levelParams = levelParams, buildLevel = buildLevel,
  tileAt = tileAt, LEVELS = LEVELS}
