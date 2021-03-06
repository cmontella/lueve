-- Imports / module wrapper
local Pkg = {}
local std = _G
local error = error
local print = print
local type = type
local tostring = tostring
local getmetatable = getmetatable
local setmetatable = setmetatable
local pairs = pairs
local ipairs = ipairs
local string = string
local table = table
local util = require("util")
local Set = require("set").Set
setfenv(1, Pkg)

OUT = "$$OUT"
IN = "$$IN"
STRONG_IN = "$$STRONG_IN"
OPT = "$$OPT"
local sigSymbols = {[OUT] = "f", [IN] = "b", [STRONG_IN] = "B", [OPT] = "?"}
local function fmtSignature(args, signature)
  local result = ""
  local multi = false
  for _, arg in ipairs(args) do
    if multi then result = result .. ", " end
    result = result .. arg .. ": " .. sigSymbols[signature[arg]]
    multi = true
  end
  return result
end

local Signature = {}
function Signature.__tostring(signature)
  local args = {}
  for arg in pairs(signature) do
    args[#args + 1] = arg
  end
  return fmtSignature(args, signature)
end

function getSignature(bindings, bound)
  local signature = setmetatable({}, Signature)
  for _, binding in ipairs(bindings) do
    if bound[binding.variable] or binding.constant then
      signature[binding.field] = IN
    else
      signature[binding.field] = OUT
    end
  end
  return signature
end
local Schema = {}
function Schema.__tostring(schema)
  local signature = fmtSignature(schema.args, schema.signature)
  local rest = schema.rest and (", ...: " .. sigSymbols[schema.rest]) or ""
  return string.format("Schema<%s, (%s%s)>", schema.name or "UNNAMED", signature, rest)
end

local function schema(args, name, kind)
  local schema = {args = {}, signature = setmetatable({}, Signature), name = name, kind = kind}
  setmetatable(schema, Schema)
  if args.name then
    schema.name = args.name
  end
  local mode = OUT
  for ix, arg in ipairs(args) do
    if arg == OUT or arg == IN or arg == STRONG_IN or arg == OPT then
      mode = arg
      if ix == #args then -- a mode token in the final slot signifies a variadic expression that takes any number of vars matching the given mode
        schema.rest = arg
      end
    else
      schema.args[#schema.args + 1] = arg
      schema.signature[arg] = mode
    end
  end
  return schema
end

local function rename(name, schema)
  local neue = util.shallowCopy(schema)
  neue.name = name
  return neue
end
local schemas = {
  unary = schema{"return", IN, "a"},
  unaryBound = schema{IN, "return", "a"},
  unaryFilter = schema{IN, "a"},
  binary = schema{"return", IN, "a", "b"},
  binaryBound = schema{IN, "return", "a", "b"},
  binaryFilter = schema{IN, "a", "b"},
  moveIn = schema{"a", IN, "b"},
  moveOut = schema{"b", IN, "a"}
}

local expressions = {
  ["+"] = {rename("plus", schemas.binary)},
  ["-"] = {rename("minus", schemas.binary)},
  ["*"] = {rename("multiply", schemas.binary)},
  ["/"] = {rename("divide", schemas.binary)},

  ["<"] = {rename("less_than", schemas.binaryFilter), rename("is_less_than", schemas.binary)},
  ["<="] = {rename("less_than_or_equal", schemas.binaryFilter), rename("is_less_than_or_equal", schemas.binary)},
  [">"] = {rename("greater_than", schemas.binaryFilter), rename("is_greater_than", schemas.binary)},
  [">="] = {rename("greater_than_or_equal", schemas.binaryFilter), rename("is_greater_than_or_equal", schemas.binary)},
  ["="] = {rename("equal", schemas.binaryFilter), rename("is_equal", schemas.binary), rename("move", schemas.moveIn), rename("move", schemas.moveOut)},
  ["!="] = {rename("not_equal", schemas.binaryFilter), rename("is_not_equal", schemas.binary)},

  concat = {schema({"return", IN}, "concat")},
  length = {rename("length", schemas.unary)},
  is = {rename("is", schemas.unary)},

  abs = {rename("abs", schemas.unary)},
  sin = {rename("sin", schemas.unary)},
  cos = {rename("cos", schemas.unary)},
  tan = {rename("tan", schemas.unary)},

  toggle = {rename("toggle", schemas.unary)},

  time = {schema({"return", OPT, "seconds", "minutes", "hours"}, "time")},

  -- Aggregates
  count = {schema({"return"}, "sum", "aggregate"), schema({IN, "return"}, "sum", "aggregate")},
  sum = {schema({"return", STRONG_IN, "a"}, "sum", "aggregate"), schema({IN, "return", STRONG_IN, "a"}, "sum", "aggregate")}
}

function getExpressions()
  local exprs = Set:new()
  for expr in pairs(expressions) do
    exprs:add(expr)
  end
  return exprs
end

function getSchemas(name)
  return expressions[name]
end

function getSchema(name, signature)
  if not expressions[name] then error("Unknown expression '" .. name .. "'") end
  if not signature then error("Must specify signature to disambiguate expression alternatives") end
  local result
  for _, schema in ipairs(expressions[name]) do
    local match = true
    local required = Set:new()
    for arg, mode in pairs(schema.signature) do
      if mode == OUT or mode == IN or mode == STRONG_IN then
        required:add(arg)
      end
    end
    for arg, mode in pairs(signature) do
      required:remove(arg)
      local schemaMode = schema.signature[arg] or schema.rest
      if schemaMode == STRONG_IN then
        schemaMode = IN
      end
      if schemaMode ~= mode and schemaMode ~= OPT then
        match = false
        break
      end
    end
    if match and required:length() == 0 then
      result = schema
      break
    end
  end
  if not result then
    local available = {}
    for _, schema in ipairs(expressions[name]) do
      available[#available + 1] = string.format("%s(%s)", name, fmtSignature(schema.args, schema.signature))
    end
    error(string.format("No matching signature for expression  %s(%s); Available signatures:\n  %s", name, signature, table.concat(available, "\n  ")))
  end

  return result
end

function getArgs(schema, bindings)
  local map = {}
  local positions = {}
  for _, binding in ipairs(bindings) do
    map[binding.field] = binding.variable or binding.constant
    positions[#positions + 1] = binding.field
  end

  local args = {}
  local fields = {}
  for _, arg in ipairs(schema.args) do
    if map[arg] then
      args[#args + 1] = map[arg]
      fields[#fields + 1] = arg
    end
  end
  if schema.rest then
    fields[#fields + 1] = "..."
    args["..."] = {}
    for _, field in ipairs(positions) do
      if not schema.signature[field] then
        args["..."][#args["..."] + 1] = map[field]
      end
    end
  end

  return args, fields
end

return Pkg
