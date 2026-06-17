-- gen-acts-registry.lua
--
-- Merge *.export.json + src/acts_metadata.h → src/acts.gen.h
--
-- Usage:
--   aseprite -b --script scripts/aesprite/gen-acts-registry.lua -- .

local function script_dir()
  local src = debug.getinfo(1, "S").source
  if src:sub(1, 1) == "@" then
    src = src:sub(2)
  end
  return src:match("^(.*[/\\])") or ""
end

local json = dofile(script_dir() .. "lib/json.lua")

local TILE_SIZE = 16
local MAX_ROOMS_PER_ACT = 16
local MAX_TUNNELS_PER_ACT = 16
local MAX_TELEPORTS_PER_ACT = 16
local MAX_SAVES_PER_ACT = 16
local SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM = 8

local function notify(title, text)
  if app.isUIAvailable then
    app.alert{ title = title, text = text }
  else
    print(title .. ": " .. text)
  end
end

local function fail(msg)
  notify("gen-acts-registry", msg)
  error(msg)
end

local function normalize_root(param)
  if param and param ~= "" then
    return param:gsub("\\", "/")
  end
  return "."
end

local function read_file(path)
  local f = io.open(path, "r")
  if not f then
    return nil
  end
  local content = f:read("*a")
  f:close()
  return content
end

local function write_file(path, content)
  local f = io.open(path, "w")
  if not f then
    fail("Cannot write: " .. path)
  end
  f:write(content)
  f:close()
end

local function parse_metadata(content)
  local rows = {}
  for id, label in content:gmatch('X%([%w_]+%s*,%s*"([^"]+)"%s*,%s*"([^"]+)"%s*%)') do
    rows[#rows + 1] = {
      id = id,
      label = label,
    }
  end
  return rows
end

local function list_export_json(layers_dir)
  local names = app.fs.listFiles(layers_dir) or {}
  local out = {}
  for _, name in ipairs(names) do
    if name:match("%.export%.json$") then
      out[#out + 1] = app.fs.joinPath(layers_dir, name)
    end
  end
  table.sort(out)
  return out
end

local function c_escape(s)
  return s:gsub("\\", "\\\\"):gsub('"', '\\"')
end

local function c_ident(prefix, id)
  return (prefix .. "_" .. id):gsub("[^%w_]", "_")
end

local function c_str_or_null(s)
  if s and s ~= "" then
    return '"' .. c_escape(s) .. '"'
  end
  return "NULL"
end

local function point_in_rect(x, y, rect)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
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

local function export_tunnels(exp)
  return exp.tunnels or exp.doors or {}
end

local function enrich_transition_spawns(exp, path)
  local room_by_id = {}
  for _, room in ipairs(exp.rooms) do
    room_by_id[room.id] = room
  end

  for _, tunnel in ipairs(export_tunnels(exp)) do
    local ra = room_by_id[tunnel.room_a]
    local rb = room_by_id[tunnel.room_b]
    if not ra or not rb then
      fail(path .. ": tunnel '" .. (tunnel.id or "?") .. "' missing spawn points")
    end
    local tunnel_rect = { x = tunnel.x, y = tunnel.y, w = tunnel.w, h = tunnel.h }
    local ia = rect_intersection(tunnel_rect, ra)
    local ib = rect_intersection(tunnel_rect, rb)
    if not ia or not ib then
      fail(path .. ": tunnel '" .. (tunnel.id or "?") .. "' could not derive spawn from room overlap")
    end
    tunnel.spawn_a = rect_spawn_point(ia)
    tunnel.spawn_b = rect_spawn_point(ib)
  end

  for _, tp in ipairs(exp.teleports) do
    tp.spawn_x = tp.x + tp.w * 0.5
    tp.spawn_y = tp.y + tp.h - SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM
  end
end

local function validate_export(exp, path)
  if not exp.rooms or #exp.rooms == 0 then
    fail(path .. ": export has no rooms")
  end
  if not exp.tunnels and not exp.doors then
    fail(path .. ": export missing tunnels array")
  end
  if not exp.teleports then
    fail(path .. ": export missing teleports array")
  end
  if not exp.saves or #exp.saves == 0 then
    fail(path .. ": export missing saves array")
  end
  local has_save_0 = false
  for _, save in ipairs(exp.saves) do
    if save.index == 0 then
      has_save_0 = true
    end
    if save.index == nil or not save.x or not save.y or not save.room then
      fail(path .. ": save point missing index, x, y, or room")
    end
  end
  if not has_save_0 then
    fail(path .. ": missing save-0")
  end
  for _, room in ipairs(exp.rooms) do
    if not room.id then
      fail(path .. ": room missing id")
    end
  end
  for _, tunnel in ipairs(export_tunnels(exp)) do
    if not tunnel.room_a or not tunnel.room_b then
      fail(path .. ": tunnel '" .. (tunnel.id or "?") .. "' missing room_a/room_b")
    end
  end
  for _, tp in ipairs(exp.teleports) do
    if not tp.id or not tp.link or not tp.room then
      fail(path .. ": teleport '" .. (tp.id or "?") .. "' missing id, link, or room")
    end
  end
end

local function project_root()
  if app.params and app.params[1] and app.params[1] ~= "" and app.params[1] ~= "--" then
    return normalize_root(app.params[1])
  end
  local dir = script_dir()
  return (dir:gsub("\\", "/")) .. "/../.."
end

local root = project_root()
local layers_dir = app.fs.joinPath(root, "resources/visual/layers")
local metadata_path = app.fs.joinPath(root, "src/acts_metadata.h")
local out_path = app.fs.joinPath(root, "src/acts.gen.h")

if not app.fs.isDirectory(layers_dir) then
  fail("Missing layers directory: " .. layers_dir)
end

local metadata_text = read_file(metadata_path)
if not metadata_text then
  fail("Missing metadata header: " .. metadata_path)
end

local metadata_rows = parse_metadata(metadata_text)
if #metadata_rows == 0 then
  fail("No ACTS_METADATA entries found in acts_metadata.h")
end

local export_by_id = {}
for _, path in ipairs(list_export_json(layers_dir)) do
  local text = read_file(path)
  if not text then
    fail("Cannot read export json: " .. path)
  end
  local data = json.decode(text)
  if not data.id then
    fail("Export json missing id: " .. path)
  end
  validate_export(data, path)
  enrich_transition_spawns(data, path)
  export_by_id[data.id] = data
end

local merged = {}
for _, meta in ipairs(metadata_rows) do
  local exp = export_by_id[meta.id]
  if not exp then
    fail("No .export.json for act id: " .. meta.id)
  end
  merged[#merged + 1] = { meta = meta, exp = exp }
  export_by_id[meta.id] = nil
end

for id in pairs(export_by_id) do
  fail("Export json has no acts_metadata.h entry: " .. id)
end

local max_w = 0
local max_h = 0
local max_room_h = 0
for _, row in ipairs(merged) do
  local w = row.exp.width or 0
  local h = row.exp.height or 0
  if w > max_w then max_w = w end
  if h > max_h then max_h = h end
  for _, room in ipairs(row.exp.rooms or {}) do
    local vh = room.view_h or room.h
    if vh and vh > max_room_h then
      max_room_h = vh
    end
  end
end
if max_room_h <= 0 then
  max_room_h = 208
end

local lines = {
  "/* Auto-generated by gen-acts-registry.lua - do not edit. */",
  "#ifndef ACTS_GEN_H",
  "#define ACTS_GEN_H",
  "",
  "#include <stdbool.h>",
  "#include <stddef.h>",
  "",
  "#define MAX_ACT_WIDTH  " .. tostring(max_w),
  "#define MAX_ACT_HEIGHT " .. tostring(max_h),
  "#define MAX_ROOM_HEIGHT " .. tostring(max_room_h),
  "#define ACT_COUNT      " .. tostring(#merged),
  "#define MAX_ROOMS_PER_ACT " .. tostring(MAX_ROOMS_PER_ACT),
  "#define MAX_TUNNELS_PER_ACT " .. tostring(MAX_TUNNELS_PER_ACT),
  "#define MAX_TELEPORTS_PER_ACT " .. tostring(MAX_TELEPORTS_PER_ACT),
  "#define MAX_SAVES_PER_ACT " .. tostring(MAX_SAVES_PER_ACT),
  "",
  "typedef struct SavePointDef {",
  "    int index;",
  "    float x, y;",
  "    const char *room_id;",
  "} SavePointDef;",
  "",
  "typedef struct RoomDef {",
  "    const char *id;",
  "    const char *name;",
  "    bool isolated;",
  "    float x, y, w, h;",
  "    float view_y, view_h;",
  "} RoomDef;",
  "",
  "typedef struct TunnelDef {",
  "    const char *id;",
  "    const char *room_a_id;",
  "    const char *room_b_id;",
  "    float x, y, w, h;",
  "    float spawn_ax, spawn_ay;",
  "    float spawn_bx, spawn_by;",
  "} TunnelDef;",
  "",
  "typedef struct TeleportDef {",
  "    const char *id;",
  "    const char *room_id;",
  "    const char *link_id;",
  "    const char *name;",
  "    float x, y, w, h;",
  "    float spawn_x, spawn_y;",
  "} TeleportDef;",
  "",
  "typedef struct ActDef {",
  "    const char *id;",
  "    const char *label;",
  "    int width;",
  "    int height;",
  "    int cols;",
  "    int rows;",
  "    const char *background_png;",
  "    const char *collision_png;",
  "    const char *gameplay_json;",
  "    const SavePointDef *saves;",
  "    int save_count;",
  "    const RoomDef *rooms;",
  "    int room_count;",
  "    const TunnelDef *tunnels;",
  "    int tunnel_count;",
  "    const TeleportDef *teleports;",
  "    int teleport_count;",
  "} ActDef;",
  "",
}

for _, row in ipairs(merged) do
  local id = row.exp.id
  local ident = c_ident("ACT", id)

  local rooms = row.exp.rooms or {}
  local tunnels = export_tunnels(row.exp)
  local teleports = row.exp.teleports or {}
  local saves = row.exp.saves or {}
  if #rooms > MAX_ROOMS_PER_ACT then
    fail("Too many rooms in " .. id .. " (max " .. MAX_ROOMS_PER_ACT .. ")")
  end
  if #tunnels > MAX_TUNNELS_PER_ACT then
    fail("Too many tunnels in " .. id .. " (max " .. MAX_TUNNELS_PER_ACT .. ")")
  end
  if #teleports > MAX_TELEPORTS_PER_ACT then
    fail("Too many teleports in " .. id .. " (max " .. MAX_TELEPORTS_PER_ACT .. ")")
  end
  if #saves > MAX_SAVES_PER_ACT then
    fail("Too many saves in " .. id .. " (max " .. MAX_SAVES_PER_ACT .. ")")
  end

  lines[#lines + 1] = "static const SavePointDef " .. ident .. "_SAVES[" .. tostring(#saves) .. "] = {"
  for i, save in ipairs(saves) do
    local comma = (i < #saves) and "," or ""
    lines[#lines + 1] = string.format(
      '    { .index = %d, .x = %.1ff, .y = %.1ff, .room_id = "%s" }%s',
      save.index, save.x, save.y, c_escape(save.room), comma)
  end
  lines[#lines + 1] = "};"
  lines[#lines + 1] = ""

  lines[#lines + 1] = "static const RoomDef " .. ident .. "_ROOMS[" .. tostring(#rooms) .. "] = {"
  for i, room in ipairs(rooms) do
    local comma = (i < #rooms) and "," or ""
    local view_y = room.view_y or room.y
    local view_h = room.view_h or room.h
    local isolated = room.isolated and "true" or "false"
    lines[#lines + 1] = string.format(
      '    { .id = "%s", .name = %s, .isolated = %s, .x = %.1ff, .y = %.1ff, .w = %.1ff, .h = %.1ff, .view_y = %.1ff, .view_h = %.1ff }%s',
      c_escape(room.id), c_str_or_null(room.name), isolated,
      room.x, room.y, room.w, room.h, view_y, view_h, comma)
  end
  lines[#lines + 1] = "};"
  lines[#lines + 1] = ""

  lines[#lines + 1] = "static const TunnelDef " .. ident .. "_TUNNELS[" .. tostring(#tunnels) .. "] = {"
  for i, tunnel in ipairs(tunnels) do
    local comma = (i < #tunnels) and "," or ""
    local sa = tunnel.spawn_a or {}
    local sb = tunnel.spawn_b or {}
    lines[#lines + 1] = string.format(
      '    { .id = "%s", .room_a_id = "%s", .room_b_id = "%s", .x = %.1ff, .y = %.1ff, .w = %.1ff, .h = %.1ff, .spawn_ax = %.1ff, .spawn_ay = %.1ff, .spawn_bx = %.1ff, .spawn_by = %.1ff }%s',
      c_escape(tunnel.id), c_escape(tunnel.room_a), c_escape(tunnel.room_b),
      tunnel.x, tunnel.y, tunnel.w, tunnel.h, sa.x or 0, sa.y or 0, sb.x or 0, sb.y or 0, comma)
  end
  lines[#lines + 1] = "};"
  lines[#lines + 1] = ""

  lines[#lines + 1] = "static const TeleportDef " .. ident .. "_TELEPORTS[" .. tostring(#teleports) .. "] = {"
  for i, tp in ipairs(teleports) do
    local comma = (i < #teleports) and "," or ""
    lines[#lines + 1] = string.format(
      '    { .id = "%s", .room_id = "%s", .link_id = "%s", .name = %s, .x = %.1ff, .y = %.1ff, .w = %.1ff, .h = %.1ff, .spawn_x = %.1ff, .spawn_y = %.1ff }%s',
      c_escape(tp.id), c_escape(tp.room), c_escape(tp.link), c_str_or_null(tp.name),
      tp.x, tp.y, tp.w, tp.h, tp.spawn_x or (tp.x + tp.w * 0.5),
      tp.spawn_y or (tp.y + tp.h - SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM), comma)
  end
  lines[#lines + 1] = "};"
  lines[#lines + 1] = ""
end

lines[#lines + 1] = "static const ActDef ACTS[ACT_COUNT] = {"

for i, row in ipairs(merged) do
  local exp = row.exp
  local meta = row.meta
  local ident = c_ident("ACT", exp.id)
  local w = exp.width
  local h = exp.height
  local tile_cols = math.floor(w / TILE_SIZE)
  local tile_rows = math.floor(h / TILE_SIZE)
  local rooms = exp.rooms or {}
  local tunnels = export_tunnels(exp)
  local teleports = exp.teleports or {}
  local saves = exp.saves or {}

  lines[#lines + 1] = "    {"
  lines[#lines + 1] = string.format('        .id = "%s",', c_escape(exp.id))
  lines[#lines + 1] = string.format('        .label = "%s",', c_escape(meta.label))
  lines[#lines + 1] = string.format("        .width = %d, .height = %d,", w, h)
  lines[#lines + 1] = string.format("        .cols = %d, .rows = %d,", tile_cols, tile_rows)
  lines[#lines + 1] = string.format('        .background_png = "%s",', c_escape(exp.background_png))
  lines[#lines + 1] = string.format('        .collision_png = "%s",', c_escape(exp.collision_png))
  local gameplay_json = "resources/visual/layers/" .. exp.id .. ".gameplay.json"
  lines[#lines + 1] = string.format('        .gameplay_json = "%s",', c_escape(gameplay_json))
  lines[#lines + 1] = string.format("        .saves = %s_SAVES,", ident)
  lines[#lines + 1] = string.format("        .save_count = %d,", #saves)
  lines[#lines + 1] = string.format("        .rooms = %s_ROOMS,", ident)
  lines[#lines + 1] = string.format("        .room_count = %d,", #rooms)
  lines[#lines + 1] = string.format("        .tunnels = %s_TUNNELS,", ident)
  lines[#lines + 1] = string.format("        .tunnel_count = %d,", #tunnels)
  lines[#lines + 1] = string.format("        .teleports = %s_TELEPORTS,", ident)
  lines[#lines + 1] = string.format("        .teleport_count = %d,", #teleports)
  if i < #merged then
    lines[#lines + 1] = "    },"
  else
    lines[#lines + 1] = "    }"
  end
end

lines[#lines + 1] = "};"
lines[#lines + 1] = ""
lines[#lines + 1] = "#endif"
lines[#lines + 1] = ""

write_file(out_path, table.concat(lines, "\n"))
notify("gen-acts-registry", "Wrote " .. out_path .. " (" .. tostring(#merged) .. " acts)")
