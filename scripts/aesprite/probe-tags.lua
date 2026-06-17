-- probe-tags.lua — dump tags, layers, and frame info for the active sprite
-- Usage: aseprite -b resources/visual/character-player.aseprite -script scripts/aesprite/probe-tags.lua

local function repr(s)
  if s == nil or s == "" then
    return "(empty)"
  end
  return string.format("%q", s)
end

local spr = app.activeSprite
if not spr then
  print("No active sprite")
  return
end

print("Sprite: " .. (spr.filename or "(unsaved)"))
print(string.format("Size: %dx%d", spr.width, spr.height))
print(string.format("Frames: %d", #spr.frames))
print(string.format("Layers: %d", #spr.layers))
print("")

print("Layers:")
for i, layer in ipairs(spr.layers) do
  local vis = layer.isVisible and "visible" or "hidden"
  print(string.format("  [%d] %s (%s)", i, repr(layer.name), vis))
end
print("")

print(string.format("Tags: %d", #spr.tags))
for i, tag in ipairs(spr.tags) do
  print(string.format("[%d] name=%s", i, repr(tag.name)))
  print(string.format("    frames=%d..%d aniDir=%s", tag.fromFrame, tag.toFrame, tostring(tag.aniDir)))
end
print("")

print(string.format("Slices: %d", #spr.slices))
for i, slice in ipairs(spr.slices) do
  local b = slice.bounds
  print(string.format("[%d] name=%s bounds=(%d,%d %dx%d)", i, repr(slice.name), b.x, b.y, b.width, b.height))
end
