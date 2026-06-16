-- slice_data.lua — parse Aseprite slice.data (User Data field) into key/value table.
--
-- Format: one key:value per line, separated by real newlines.
--   name:"Act Start"
--   link:teleport-ir1-a
--
-- Returns: values table, errors table (slice_name prefixed in messages)

local slice_data = {}

local ALLOWED_KEYS = {
  name = true,
  link = true,
}

local function trim(s)
  return (s or ""):gsub("^%s+", ""):gsub("%s+$", "")
end

local function unquote(value)
  local inner = value:match('^"(.*)"$')
  if inner ~= nil then
    return inner
  end
  return value
end

function slice_data.parse(data, slice_name)
  local values = {}
  local errors = {}
  local prefix = slice_name and ("slice '" .. slice_name .. "': ") or ""

  if data == nil or data == "" then
    return values, errors
  end

  local line_num = 0
  for line in (data .. "\n"):gmatch("(.-)\n") do
    line_num = line_num + 1
    line = trim(line)
    if line ~= "" then
      local key, value = line:match("^([%w_%-]+)%s*:%s*(.+)$")
      if not key then
        errors[#errors + 1] = prefix .. "userdata line " .. line_num
          .. ": expected key:value, got " .. string.format("%q", line)
      else
        if not ALLOWED_KEYS[key] then
          errors[#errors + 1] = prefix .. "userdata line " .. line_num
            .. ": unknown key '" .. key .. "' (allowed: name, link)"
        end
        if values[key] ~= nil then
          errors[#errors + 1] = prefix .. "userdata: duplicate key '" .. key .. "'"
        end
        values[key] = unquote(trim(value))
      end
    end
  end

  return values, errors
end

function slice_data.require_keys(values, required, slice_name)
  local errors = {}
  local prefix = slice_name and ("slice '" .. slice_name .. "': ") or ""
  for _, key in ipairs(required) do
    if values[key] == nil or values[key] == "" then
      errors[#errors + 1] = prefix .. "userdata missing required key '" .. key .. "'"
    end
  end
  return errors
end

function slice_data.reject_keys(values, forbidden, slice_name)
  local errors = {}
  local prefix = slice_name and ("slice '" .. slice_name .. "': ") or ""
  for _, key in ipairs(forbidden) do
    if values[key] ~= nil and values[key] ~= "" then
      errors[#errors + 1] = prefix .. "userdata must not contain key '" .. key .. "'"
    end
  end
  return errors
end

return slice_data
