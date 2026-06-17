-- write-collision-layer.lua — patch the editor collision layer (primitives) from a PNG
--
-- Usage:
--   aseprite -b -script-param aseprite=<path> -script-param png=<path> \
--     [-script-param layer=primitives] -script scripts/aesprite/write-collision-layer.lua
--
-- Writes ONLY to primitives/collision — never base (base holds tile art).

local EDIT_LAYER_ALIASES = { "primitives", "collision" }

local function fail(msg)
  print("write-collision: " .. msg)
  error(msg)
end

local function normalizeName(name)
  local n = string.lower(name or "")
  n = n:gsub("^%s+", ""):gsub("%s+$", "")
  return n
end

local function nameMatches(name, aliases)
  local n = normalizeName(name)
  for _, alias in ipairs(aliases) do
    if n == normalizeName(alias) then
      return true
    end
  end
  return false
end

local function isGroupLayer(obj)
  if not obj then return false end
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

local function findEditLayer(layers, preferred)
  if preferred and preferred ~= "" then
    for _, layer in ipairs(layers) do
      if not isGroupLayer(layer) and normalizeName(layer.name) == normalizeName(preferred) then
        return layer
      end
    end
  end
  for _, layer in ipairs(layers) do
    if not isGroupLayer(layer) and nameMatches(layer.name, EDIT_LAYER_ALIASES) then
      return layer
    end
  end
  return nil
end

local ase_path = app.params and app.params.aseprite
local png_path = app.params and app.params.png
local layer_pref = app.params and app.params.layer

if not ase_path or ase_path == "" or not png_path or png_path == "" then
  fail("usage: -script-param aseprite=<path> -script-param png=<path> [-script-param layer=primitives]")
end

if not app.fs.isFile(ase_path) then
  fail("aseprite file not found: " .. ase_path)
end
if not app.fs.isFile(png_path) then
  fail("png file not found: " .. png_path)
end

local spr = app.open(ase_path)
if not spr then
  fail("failed to open " .. ase_path)
end

app.activeSprite = spr
if #spr.frames > 0 then
  app.activeFrame = 1
end

local all_layers = {}
collectLayers(spr.layers, all_layers)

local layer = findEditLayer(all_layers, layer_pref or "primitives")
if not layer then
  layer = spr:newLayer("primitives")
  print("write-collision: created empty 'primitives' layer")
end

local spr_w = spr.width
local spr_h = spr.height

local function load_png_image(path)
  local png_sprite = app.open(path)
  if not png_sprite then
    return nil
  end
  app.activeSprite = png_sprite
  app.activeFrame = 1
  local png_layers = {}
  collectLayers(png_sprite.layers, png_layers)
  local img = nil
  for _, lyr in ipairs(png_layers) do
    if not isGroupLayer(lyr) then
      local cel = lyr:cel(1)
      if cel and cel.image then
        img = Image(cel.image)
        break
      end
    end
  end
  png_sprite:close()
  app.activeSprite = spr
  app.activeFrame = 1
  return img
end

local img = load_png_image(png_path)
if not img then
  spr:close()
  fail("failed to load png: " .. png_path)
end

app.activeSprite = spr
app.activeFrame = 1

if img.width ~= spr_w or img.height ~= spr_h then
  spr:close()
  fail(string.format("png size %dx%d does not match sprite %dx%d",
    img.width, img.height, spr_w, spr_h))
end

local cel = layer:cel(1)
if cel then
  cel.image = img
else
  spr:newCel(layer, 1, img, Point(0, 0))
end

local layer_name = layer.name
local saved = pcall(function() spr:save() end)
if not saved then
  spr:saveAs(ase_path)
end
spr:close()
print("write-collision: updated layer '" .. layer_name .. "' in " .. ase_path)
