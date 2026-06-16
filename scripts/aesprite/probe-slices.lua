-- probe-slices.lua — dump slice.name vs slice.data for the active sprite
-- Usage: aseprite -b resources/visual/green-act.aseprite -script scripts/aesprite/probe-slices.lua

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
print(string.format("Slices: %d", #spr.slices))
print("")

for i, slice in ipairs(spr.slices) do
  local b = slice.bounds
  print(string.format("[%d] name=%s", i, repr(slice.name)))
  print(string.format("    data=%s", repr(slice.data)))
  print(string.format("    bounds=(%d,%d %dx%d)", b.x, b.y, b.width, b.height))
  print("")
end
