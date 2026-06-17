-- probe-layers.lua
local function isGroupLayer(obj)
  if not obj then return false end
  local ok, isGroup = pcall(function() return obj.isGroup end)
  return ok and isGroup
end

local function dumpLayers(layers, indent)
  indent = indent or ""
  for _, layer in ipairs(layers) do
    local kind = isGroupLayer(layer) and "group" or "layer"
    print(string.format("%s[%s] %s", indent, kind, layer.name))
    if isGroupLayer(layer) then dumpLayers(layer.layers, indent .. "  ") end
  end
end

local spr = app.activeSprite
if not spr then print("No sprite") return end
print(string.format("Sprite: %s %dx%d", spr.filename or "?", spr.width, spr.height))
print("Layers:")
dumpLayers(spr.layers, "")
print("Slices:")
for _, slice in ipairs(spr.slices) do
  local b = slice.bounds
  print(string.format("  %s (%d,%d %dx%d)", slice.name, b.x, b.y, b.width, b.height))
end
