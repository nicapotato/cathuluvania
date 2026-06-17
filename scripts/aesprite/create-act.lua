-- create-act.lua — scaffold a new empty act .aseprite with standard layers + tileset template
--
-- Usage:
--   aseprite -b -script-param act_id=<id> -script-param width=<w> -script-param height=<h> \
--     [-script-param template=<path>] -script scripts/aesprite/create-act.lua
--
-- Creates resources/visual/<act_id>.aseprite with layers:
--   background, parralax, tileset (from template), base, primitives, Layer 1
-- Slices: r-1 (full canvas), save-0 (center-bottom)

local TILE_SIZE = 16
local SPAWN_CENTER_OFFSET = 8

local function script_dir()
  local src = debug.getinfo(1, "S").source
  if src:sub(1, 1) == "@" then src = src:sub(2) end
  return src:match("^(.*[/\\])") or ""
end

local function project_root()
  local dir = script_dir()
  return (dir:gsub("\\", "/")) .. "/../.."
end

local function fail(msg)
  print("create-act: " .. msg)
  error(msg)
end

local function find_layer_by_name(layers, name)
  for _, layer in ipairs(layers) do
    if layer.name == name then
      return layer
    end
    if layer.isGroup then
      local found = find_layer_by_name(layer.layers, name)
      if found then return found end
    end
  end
  return nil
end

local act_id = app.params and (app.params.act_id or app.params[1])
local width = app.params and tonumber(app.params.width or app.params[2])
local height = app.params and tonumber(app.params.height or app.params[3])
local template_path = app.params and (app.params.template or app.params[4])

if not act_id or act_id == "" or not width or not height then
  fail("usage: create-act.lua <act_id> <width> <height> [template_path]")
end

if width % TILE_SIZE ~= 0 or height % TILE_SIZE ~= 0 then
  fail("width and height must be divisible by " .. tostring(TILE_SIZE))
end

local root = project_root()
if not template_path or template_path == "" then
  template_path = app.fs.joinPath(root, "resources/visual/green-act.aseprite.bk")
end

local out_path = app.fs.joinPath(root, "resources/visual/" .. act_id .. ".aseprite")

local spr = Sprite(width, height, ColorMode.RGB)
app.activeSprite = spr

local layer_names = { "background", "parralax", "tileset", "base", "primitives", "Layer 1" }
for _, name in ipairs(layer_names) do
  spr:newLayer(name)
end

if app.fs.isFile(template_path) then
  local template = app.open(template_path)
  if template then
    local tileset_layer = find_layer_by_name(template.layers, "tileset")
    if tileset_layer then
      local src_cel = tileset_layer:cel(1)
      if src_cel and src_cel.image then
        local dst_layer = find_layer_by_name(spr.layers, "tileset")
        if dst_layer then
          spr:newCel(dst_layer, 1, src_cel.image, Point(0, 0))
        end
      end
    end
    template:close()
  end
end

app.activeSprite = spr

local room_slice = spr:newSlice(Rectangle(0, 0, width, height))
room_slice.name = "r-1"
room_slice.data = 'name:"Start"'

local save_size = 18
local save_x = math.floor(width * 0.5 - save_size * 0.5)
local save_y = height - save_size - 8
local save_slice = spr:newSlice(Rectangle(save_x, save_y, save_size, save_size))
save_slice.name = "save-0"

app.activeSprite = spr
spr:saveAs(out_path)
print("create-act: wrote " .. out_path)
