-- export-act-level.lua
--
-- Cathuluvania — bake Aseprite layers to game PNGs + export manifest.
--
-- Usage (CLI / Makefile):
--   aseprite -b resources/visual/green-act.aseprite \
--     -script scripts/aesprite/export-act-level.lua
--
-- Expected layers:
--   backdrop  : background, parallax, parralax
--   render    : base, earth-tileset (tile art → <id>.png, also opaque pixels = solid)
--   editor    : primitives, collision (in-game B/E tools write primitives only)
--
-- Expected slices (document-level):
--   r-<n>              connected room; userdata optional: name:"..."
--   ir-<n>             isolated room; userdata optional: name:"..."
--   door-<id> / tunnel-<id>  connector between two rooms; rooms inferred from slice bounds (no userdata)
--   teleport-<id>      userdata required: link:<target>\nname:"..." (name optional)
--   save-<n>           save point (slice center, n from 0; save-0 required; no userdata)
--
-- Tunnel/teleport spawn Y: slice bottom minus SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM (player center).
-- Output under resources/visual/layers/:
--   <id>-background.png, <id>.png, <id>-primitives.png, <id>.export.json

local SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM = 8

local function script_dir()
  local src = debug.getinfo(1, "S").source
  if src:sub(1, 1) == "@" then
    src = src:sub(2)
  end
  return src:match("^(.*[/\\])") or ""
end

local json = dofile(script_dir() .. "lib/json.lua")
local slice_data = dofile(script_dir() .. "lib/slice_data.lua")

local CONFIG = {
  outputSubdir = "layers",
  tile_size = 16,
  useFirstFrame = true,
  view_padding_top = 16,
}

local BACKGROUND_ALIASES = { "background", "parallax", "parralax" }
local RENDER_ALIASES = { "base", "earth-tileset", "earth_tileset" }
local PRIMITIVES_ALIASES = { "primitives", "collision" }

local function normalizeName(name)
  local n = string.lower(name or "")
  n = n:gsub("^%s+", ""):gsub("%s+$", "")
  return n
end

local function nameMatches(name, aliases)
  local n = normalizeName(name)
  for _, alias in ipairs(aliases) do
    local a = normalizeName(alias)
    if n == a then
      return true
    end
    if a == "parallax" and (n:sub(1, 8) == "parralax" or n:sub(1, 8) == "parallax") then
      return true
    end
    if a == "base" and (n == "earth-tileset" or n == "earth_tileset") then
      return true
    end
  end
  return false
end

local function isGroupLayer(obj)
  if not obj then
    return false
  end
  local ok, isGroup = pcall(function() return obj.isGroup end)
  return ok and isGroup
end

local function collectLayers(layers, out)
  for _, layer in ipairs(layers) do
    table.insert(out, layer)
    if isGroupLayer(layer) then
      collectLayers(layer.layers, out)
    end
  end
end

local function saveVisibility(layers, vis)
  for _, layer in ipairs(layers) do
    vis[layer] = layer.isVisible
    if isGroupLayer(layer) then
      saveVisibility(layer.layers, vis)
    end
  end
end

local function restoreVisibility(layers, vis)
  for _, layer in ipairs(layers) do
    if vis[layer] ~= nil then
      layer.isVisible = vis[layer]
    end
    if isGroupLayer(layer) then
      restoreVisibility(layer.layers, vis)
    end
  end
end

local function parentChain(layer)
  local chain = {}
  local parent = layer.parent
  while isGroupLayer(parent) do
    table.insert(chain, parent)
    parent = parent.parent
  end
  return chain
end

local function setExportVisibility(allLayers, aliases)
  for _, layer in ipairs(allLayers) do
    layer.isVisible = false
  end

  local shown = 0
  for _, layer in ipairs(allLayers) do
    if not isGroupLayer(layer) and nameMatches(layer.name, aliases) then
      layer.isVisible = true
      shown = shown + 1
      for _, parent in ipairs(parentChain(layer)) do
        parent.isVisible = true
      end
    end
  end
  return shown
end

local function fileStem(path)
  local name = app.fs.fileName(path)
  return name:match("^(.+)%.[^%.]+$") or name
end

local function projectRootFromSpritePath(spritePath)
  local visualDir = app.fs.filePath(spritePath)
  local resourcesDir = app.fs.filePath(visualDir)
  return app.fs.filePath(resourcesDir)
end

local function normalize_path(p)
  return (p or ""):gsub("\\", "/")
end

local function pathRelativeToRoot(absPath, root)
  local abs = normalize_path(absPath)
  local base = normalize_path(root)
  if base:sub(-1) == "/" then
    base = base:sub(1, -2)
  end
  if abs:sub(1, #base) == base then
    local rel = abs:sub(#base + 1)
    rel = rel:gsub("^/", "")
    return rel
  end
  return abs
end

local function write_text_file(path, content)
  local f = io.open(path, "w")
  if not f then
    return false
  end
  f:write(content)
  f:close()
  return true
end

local function notify(title, text)
  print(title .. ": " .. text)
end

local function fail(msg)
  notify("Export Act Level", msg)
  error(msg)
end

local function append_all(dest, src)
  for _, v in ipairs(src) do
    dest[#dest + 1] = v
  end
end

local function exportPass(sprite, allLayers, visBackup, aliases, outPath)
  local shown = setExportVisibility(allLayers, aliases)
  if shown == 0 then
    return false, "no matching layers (" .. table.concat(aliases, ", ") .. ")"
  end

  sprite:saveCopyAs(outPath)
  restoreVisibility(sprite.layers, visBackup)
  return true, outPath
end

local function slice_center(slice)
  local b = slice.bounds
  return {
    x = b.x + b.width * 0.5,
    y = b.y + b.height * 0.5,
  }
end

local function rect_from_bounds(b)
  return { x = b.x, y = b.y, w = b.width, h = b.height }
end

local function rect_intersection(a, b)
  local x1 = math.max(a.x, b.x)
  local y1 = math.max(a.y, b.y)
  local x2 = math.min(a.x + a.w, b.x + b.w)
  local y2 = math.min(a.y + a.h, b.y + b.h)
  if x2 <= x1 or y2 <= y1 then
    return nil
  end
  return { x = x1, y = y1, w = x2 - x1, h = y2 - y1 }
end

local function rect_spawn_point(r)
  return {
    x = r.x + r.w * 0.5,
    y = r.y + r.h - SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM,
  }
end

local function aabb_overlap(a, b)
  return a.x < b.x + b.w and a.x + a.w > b.x and a.y < b.y + b.h and a.y + a.h > b.y
end

local function point_in_rect(x, y, rect)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
end

local function clip_room_to_canvas(room, canvas_h)
  if room.y + room.h > canvas_h then
    room.h = canvas_h - room.y
  end
  if room.h < 0 then
    room.h = 0
  end
end

local function finalize_room_views(sprite, rooms, warnings)
  for _, room in ipairs(rooms) do
    clip_room_to_canvas(room, sprite.height)
    if room.y + room.h > sprite.height then
      warnings[#warnings + 1] = "slice '" .. room.id .. "': bounds exceed canvas height (clipped)"
    end
    -- view_y/view_h are refined at runtime from the baked collision PNG (level.c).
    room.view_y = room.y
    room.view_h = room.h
  end
end

local function rooms_overlapping_rect(rooms, rect)
  local out = {}
  for _, room in ipairs(rooms) do
    if aabb_overlap(room, rect) then
      out[#out + 1] = room.id
    end
  end
  return out
end

local function find_room_by_id(rooms, id)
  for _, room in ipairs(rooms) do
    if room.id == id then
      return room
    end
  end
  return nil
end

local function compute_tunnel_spawns(tunnel, rooms, errors)
  local ra = find_room_by_id(rooms, tunnel.room_a)
  local rb = find_room_by_id(rooms, tunnel.room_b)
  if not ra or not rb then
    return
  end

  local ia = rect_intersection(tunnel, ra)
  local ib = rect_intersection(tunnel, rb)
  if not ia then
    errors[#errors + 1] = "slice '" .. tunnel.slice_name .. "': tunnel bounds do not overlap room "
      .. tunnel.room_a
    return
  end
  if not ib then
    errors[#errors + 1] = "slice '" .. tunnel.slice_name .. "': tunnel bounds do not overlap room "
      .. tunnel.room_b
    return
  end

  tunnel.spawn_a = rect_spawn_point(ia)
  tunnel.spawn_b = rect_spawn_point(ib)
end

local function collect_slices(sprite, width, height, errors, warnings)
  local rooms = {}
  local tunnels = {}
  local teleports = {}
  local saves = {}
  local save_indices = {}
  local seen_names = {}

  for _, slice in ipairs(sprite.slices) do
    local name = slice.name
    local b = slice.bounds
    local rect = rect_from_bounds(b)
    local userdata, parse_errs = slice_data.parse(slice.data, name)
    append_all(errors, parse_errs)

    if seen_names[name] then
      errors[#errors + 1] = "duplicate slice name: " .. name
    end
    seen_names[name] = true

    local entry = {
      slice_name = name,
      x = rect.x,
      y = rect.y,
      w = rect.w,
      h = rect.h,
    }

    if name:match("^r%-%d+$") then
      append_all(errors, slice_data.reject_keys(userdata, { "link" }, name))
      entry.id = name
      entry.isolated = false
      entry.name = userdata.name
      rooms[#rooms + 1] = entry

    elseif name:match("^ir%-%d+$") then
      append_all(errors, slice_data.reject_keys(userdata, { "link" }, name))
      entry.id = name
      entry.isolated = true
      entry.name = userdata.name
      rooms[#rooms + 1] = entry

    elseif name:match("^door%-") or name:match("^tunnel%-") then
      if slice.data and slice.data ~= "" then
        errors[#errors + 1] = "slice '" .. name .. "': tunnel slices must have empty User Data"
      end
      entry.id = name
      tunnels[#tunnels + 1] = entry

    elseif name:match("^teleport%-") then
      append_all(errors, slice_data.require_keys(userdata, { "link" }, name))
      entry.id = name
      entry.link = userdata.link
      entry.name = userdata.name
      entry.spawn_x = rect.x + rect.w * 0.5
      entry.spawn_y = rect.y + rect.h - SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM
      teleports[#teleports + 1] = entry

    elseif name:match("^save%-(%d+)$") then
      if slice.data and slice.data ~= "" then
        errors[#errors + 1] = "slice '" .. name .. "': save slices must have empty User Data"
      end
      local idx = tonumber(name:match("^save%-(%d+)$"))
      if save_indices[idx] then
        errors[#errors + 1] = "duplicate save index: save-" .. tostring(idx)
      end
      save_indices[idx] = true
      local c = slice_center(slice)
      saves[#saves + 1] = {
        index = idx,
        slice_name = name,
        x = c.x,
        y = c.y,
      }

    elseif normalizeName(name) == "spawn" then
      errors[#errors + 1] = "slice 'spawn': deprecated — rename to save-0"

    elseif name:match("^room%-") then
      errors[#errors + 1] = "slice '" .. name
        .. "': deprecated naming (use r-<n>, door-<id>/tunnel-<id>, teleport-<id>)"

    else
      errors[#errors + 1] = "slice '" .. name .. "': unrecognized slice name"
    end
  end

  table.sort(rooms, function(a, b) return a.id < b.id end)
  table.sort(tunnels, function(a, b) return a.slice_name < b.slice_name end)
  table.sort(teleports, function(a, b) return a.id < b.id end)

  if #rooms == 0 then
    errors[#errors + 1] = "no r-<n> or ir-<n> room slices found"
  end

  local room_ids = {}
  for _, room in ipairs(rooms) do
    if room_ids[room.id] then
      errors[#errors + 1] = "duplicate room id: " .. room.id
    end
    room_ids[room.id] = true
  end

  for _, tunnel in ipairs(tunnels) do
    local overlaps = rooms_overlapping_rect(rooms, tunnel)
    if #overlaps ~= 2 then
      errors[#errors + 1] = "slice '" .. tunnel.slice_name .. "': tunnel must overlap exactly 2 rooms, "
        .. "found " .. #overlaps .. ( #overlaps > 0 and (" (" .. table.concat(overlaps, ", ") .. ")") or "" )
    else
      table.sort(overlaps)
      tunnel.room_a = overlaps[1]
      tunnel.room_b = overlaps[2]
      compute_tunnel_spawns(tunnel, rooms, errors)
    end
  end

  local teleport_ids = {}
  for _, tp in ipairs(teleports) do
    if teleport_ids[tp.id] then
      errors[#errors + 1] = "duplicate teleport id: " .. tp.id
    end
    teleport_ids[tp.id] = true
  end

  for _, tp in ipairs(teleports) do
    if not teleport_ids[tp.link] then
      errors[#errors + 1] = "slice '" .. tp.id .. "': link target '" .. tp.link
        .. "' not found in this act"
    end
    local c = { x = tp.x + tp.w * 0.5, y = tp.y + tp.h * 0.5 }
    tp.room = nil
    for _, room in ipairs(rooms) do
      if point_in_rect(c.x, c.y, room) then
        if tp.room then
          errors[#errors + 1] = "slice '" .. tp.id .. "': center is inside multiple rooms"
        end
        tp.room = room.id
      end
    end
    if not tp.room then
      errors[#errors + 1] = "slice '" .. tp.id .. "': center (" .. math.floor(c.x) .. ","
        .. math.floor(c.y) .. ") is not inside any room"
    end
  end

  for _, tp in ipairs(teleports) do
    if tp.link == tp.id then
      errors[#errors + 1] = "slice '" .. tp.id .. "': link cannot point to itself"
    end
  end

  table.sort(saves, function(a, b) return a.index < b.index end)

  if not save_indices[0] then
    errors[#errors + 1] = "missing save-0 slice (required default save point)"
  end

  for _, save in ipairs(saves) do
    local save_room = nil
    for _, room in ipairs(rooms) do
      if point_in_rect(save.x, save.y, room) then
        if save_room then
          errors[#errors + 1] = "slice '" .. save.slice_name .. "': center is inside multiple rooms"
        end
        save_room = room.id
      end
    end
    if not save_room then
      errors[#errors + 1] = "slice '" .. save.slice_name .. "': center (" .. math.floor(save.x) .. ","
        .. math.floor(save.y) .. ") is not inside any room"
    else
      save.room = save_room
    end
  end

  return rooms, tunnels, teleports, saves
end

local spr = app.activeSprite
if not spr then
  return notify("Export Act Level", "No active sprite.")
end

if not spr.filename or spr.filename == "" then
  return notify("Export Act Level", "Save the .aseprite file first.")
end

if CONFIG.useFirstFrame and #spr.frames > 0 then
  app.activeFrame = 1
end

local stem = fileStem(spr.filename)
local bgName = stem .. "-background.png"
local collisionName = stem .. ".png"
local primitivesName = stem .. "-primitives.png"
local exportJsonName = stem .. ".export.json"
local outDir = app.fs.joinPath(app.fs.filePath(spr.filename), CONFIG.outputSubdir)
local projectRoot = projectRootFromSpritePath(spr.filename)

if not app.fs.isDirectory(outDir) then
  app.fs.makeDirectory(outDir)
end

local bgPath = app.fs.joinPath(outDir, bgName)
local collisionPath = app.fs.joinPath(outDir, collisionName)
local primitivesPath = app.fs.joinPath(outDir, primitivesName)
local exportJsonPath = app.fs.joinPath(outDir, exportJsonName)

local allLayers = {}
collectLayers(spr.layers, allLayers)

local visBackup = {}
saveVisibility(spr.layers, visBackup)

local results = {}
local okBg, bgMsg = exportPass(spr, allLayers, visBackup, BACKGROUND_ALIASES, bgPath)
table.insert(results, (okBg and "OK  " or "FAIL") .. "  " .. bgName .. (okBg and "" or " — " .. bgMsg))

saveVisibility(spr.layers, visBackup)
local okCol, colMsg = exportPass(spr, allLayers, visBackup, RENDER_ALIASES, collisionPath)
table.insert(results, (okCol and "OK  " or "FAIL") .. "  " .. collisionName .. (okCol and "" or " — " .. colMsg))

saveVisibility(spr.layers, visBackup)
local okPrim, primMsg = exportPass(spr, allLayers, visBackup, PRIMITIVES_ALIASES, primitivesPath)
table.insert(results, (okPrim and "OK  " or "SKIP") .. "  " .. primitivesName
  .. (okPrim and "" or " — " .. primMsg))

restoreVisibility(spr.layers, visBackup)

local errors = {}
local warnings = {}
if spr.width % CONFIG.tile_size ~= 0 then
  warnings[#warnings + 1] = "width " .. spr.width .. " not divisible by tile_size " .. CONFIG.tile_size
end
if spr.height % CONFIG.tile_size ~= 0 then
  warnings[#warnings + 1] = "height " .. spr.height .. " not divisible by tile_size " .. CONFIG.tile_size
end

local rooms, tunnels, teleports, saves = collect_slices(spr, spr.width, spr.height, errors, warnings)

finalize_room_views(spr, rooms, warnings)

local okExport = okBg and okCol and #errors == 0

if okExport then
  local room_export = {}
  for _, room in ipairs(rooms) do
    room_export[#room_export + 1] = {
      id = room.id,
      name = room.name,
      isolated = room.isolated,
      x = room.x,
      y = room.y,
      w = room.w,
      h = room.h,
      view_y = room.view_y,
      view_h = room.view_h,
    }
  end

  local tunnel_export = {}
  for _, tunnel in ipairs(tunnels) do
    tunnel_export[#tunnel_export + 1] = {
      id = tunnel.id,
      room_a = tunnel.room_a,
      room_b = tunnel.room_b,
      x = tunnel.x,
      y = tunnel.y,
      w = tunnel.w,
      h = tunnel.h,
      spawn_a = tunnel.spawn_a,
      spawn_b = tunnel.spawn_b,
    }
  end

  local teleport_export = {}
  for _, tp in ipairs(teleports) do
    teleport_export[#teleport_export + 1] = {
      id = tp.id,
      room = tp.room,
      link = tp.link,
      name = tp.name,
      x = tp.x,
      y = tp.y,
      w = tp.w,
      h = tp.h,
      spawn_x = tp.spawn_x,
      spawn_y = tp.spawn_y,
    }
  end

  local save_export = {}
  for _, save in ipairs(saves) do
    save_export[#save_export + 1] = {
      index = save.index,
      x = save.x,
      y = save.y,
      room = save.room,
    }
  end

  local manifest = {
    id = stem,
    width = spr.width,
    height = spr.height,
    tile_size = CONFIG.tile_size,
    background_png = pathRelativeToRoot(bgPath, projectRoot),
    collision_png = pathRelativeToRoot(collisionPath, projectRoot),
    rooms = room_export,
    tunnels = tunnel_export,
    teleports = teleport_export,
    saves = save_export,
  }

  if write_text_file(exportJsonPath, json.encode(manifest)) then
    table.insert(results, "OK    " .. exportJsonName)
    table.insert(results, string.format("      %d rooms, %d tunnels, %d teleports, %d saves",
      #rooms, #tunnels, #teleports, #saves))
  else
    table.insert(results, "FAIL  " .. exportJsonName .. " — cannot write file")
    okExport = false
  end
else
  table.insert(results, "FAIL  " .. exportJsonName .. " — slice validation failed")
end

for _, e in ipairs(errors) do
  table.insert(results, "ERROR " .. e)
end
for _, w in ipairs(warnings) do
  table.insert(results, "WARN  " .. w)
end

local summary = table.concat(results, "\n") .. "\n\n→ " .. outDir
if okExport then
  notify("Export Act Level", summary)
else
  local sprite_path = spr.filename or "(unsaved)"
  local help = table.concat({
    "",
    "Fix the issues above in Aseprite:",
    "  File: " .. sprite_path,
    "  Slices panel — unique names, bounds overlapping rooms/tunnels, User Data keys",
    "  Saves: name save-<n> (save-0 required), slice center = player position",
    "  See slice rules in: scripts/aesprite/export-act-level.lua (file header)",
    "Then re-run: make assets",
  }, "\n")
  notify("Export Act Level (errors)", summary .. help)
  fail("export validation failed for " .. stem)
end
