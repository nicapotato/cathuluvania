-- JSON encode/decode for export manifests (objects, arrays, strings, numbers, null).

local json = {}

local function escape_str(s)
  return s:gsub("\\", "\\\\"):gsub('"', '\\"'):gsub("\n", "\\n"):gsub("\r", "\\r")
end

local function is_array(t)
  if type(t) ~= "table" then
    return false
  end
  local n = #t
  if n == 0 then
    return next(t) == nil
  end
  for k in pairs(t) do
    if type(k) ~= "number" or k < 1 or k > n or math.floor(k) ~= k then
      return false
    end
  end
  return true
end

local encode_value

encode_value = function(v, indent)
  indent = indent or 0
  local pad = string.rep("  ", indent)
  local pad_in = string.rep("  ", indent + 1)

  local tv = type(v)
  if v == nil then
    return "null"
  elseif tv == "string" then
    return string.format('"%s"', escape_str(v))
  elseif tv == "number" then
    if math.type and math.type(v) == "integer" then
      return string.format("%d", v)
    end
    return string.format("%g", v)
  elseif tv == "boolean" then
    return v and "true" or "false"
  elseif tv == "table" then
    if is_array(v) then
      if #v == 0 then
        return "[]"
      end
      local parts = { "[\n" }
      for i, item in ipairs(v) do
        parts[#parts + 1] = pad_in .. encode_value(item, indent + 1)
        if i < #v then
          parts[#parts] = parts[#parts] .. ","
        end
        parts[#parts + 1] = "\n"
      end
      parts[#parts + 1] = pad .. "]"
      return table.concat(parts)
    end

    local keys = {}
    for k in pairs(v) do
      keys[#keys + 1] = k
    end
    table.sort(keys, function(a, b)
      return tostring(a) < tostring(b)
    end)

    if #keys == 0 then
      return "{}"
    end

    local parts = { "{\n" }
    for i, k in ipairs(keys) do
      parts[#parts + 1] = string.format('%s"%s": %s', pad_in, tostring(k), encode_value(v[k], indent + 1))
      if i < #keys then
        parts[#parts] = parts[#parts] .. ","
      end
      parts[#parts + 1] = "\n"
    end
    parts[#parts + 1] = pad .. "}"
    return table.concat(parts)
  end

  error("json.encode: unsupported type " .. tv)
end

function json.encode(obj)
  return encode_value(obj, 0)
end

local function skip_ws(text, pos)
  while true do
    local c = text:sub(pos, pos)
    if c == "" or not c:match("%s") then
      return pos
    end
    pos = pos + 1
  end
end

local decode_value

decode_value = function(text, pos)
  pos = skip_ws(text, pos)
  local c = text:sub(pos, pos)
  if c == "{" then
    local obj = {}
    pos = pos + 1
    pos = skip_ws(text, pos)
    if text:sub(pos, pos) == "}" then
      return obj, pos + 1
    end
    while true do
      pos = skip_ws(text, pos)
      if text:sub(pos, pos) ~= '"' then
        error("json.decode: expected string key at " .. pos)
      end
      local key_end = text:find('"', pos + 1, true)
      while text:sub(key_end - 1, key_end - 1) == "\\" do
        key_end = text:find('"', key_end + 1, true)
      end
      local key = text:sub(pos + 1, key_end - 1)
      pos = key_end + 1
      pos = skip_ws(text, pos)
      if text:sub(pos, pos) ~= ":" then
        error("json.decode: expected ':' at " .. pos)
      end
      pos = skip_ws(text, pos + 1)
      local val
      val, pos = decode_value(text, pos)
      obj[key] = val
      pos = skip_ws(text, pos)
      local sep = text:sub(pos, pos)
      if sep == "}" then
        return obj, pos + 1
      elseif sep == "," then
        pos = pos + 1
      else
        error("json.decode: expected ',' or '}' at " .. pos)
      end
    end
  elseif c == "[" then
    local arr = {}
    pos = pos + 1
    pos = skip_ws(text, pos)
    if text:sub(pos, pos) == "]" then
      return arr, pos + 1
    end
    while true do
      local val
      val, pos = decode_value(text, pos)
      arr[#arr + 1] = val
      pos = skip_ws(text, pos)
      local sep = text:sub(pos, pos)
      if sep == "]" then
        return arr, pos + 1
      elseif sep == "," then
        pos = pos + 1
      else
        error("json.decode: expected ',' or ']' at " .. pos)
      end
    end
  elseif c == '"' then
    local i = pos + 1
    local out = {}
    while true do
      local ch = text:sub(i, i)
      if ch == "" then
        error("json.decode: unterminated string")
      elseif ch == '"' then
        return table.concat(out), i + 1
      elseif ch == "\\" then
        local esc = text:sub(i + 1, i + 1)
        if esc == "n" then out[#out + 1] = "\n"
        elseif esc == "r" then out[#out + 1] = "\r"
        elseif esc == "t" then out[#out + 1] = "\t"
        elseif esc == '"' or esc == "\\" or esc == "/" then out[#out + 1] = esc
        else out[#out + 1] = esc
        end
        i = i + 2
      else
        out[#out + 1] = ch
        i = i + 1
      end
    end
  elseif c == "t" and text:sub(pos, pos + 3) == "true" then
    return true, pos + 4
  elseif c == "f" and text:sub(pos, pos + 4) == "false" then
    return false, pos + 5
  elseif c == "n" and text:sub(pos, pos + 3) == "null" then
    return nil, pos + 4
  else
    local num = text:match("^%-?[%d%.]+[%deE%-+%d]*", pos)
    if num then
      return tonumber(num), pos + #num
    end
    error("json.decode: unexpected token at " .. pos .. ": " .. c)
  end
end

function json.decode(text)
  local val, pos = decode_value(text, 1)
  pos = skip_ws(text, pos)
  if pos <= #text then
    error("json.decode: trailing data at " .. pos)
  end
  return val
end

return json
