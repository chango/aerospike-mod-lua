
local math = require "test_math"

function record(r)
    return r.a
end

function sum(r,a,b)
    return math.add(a,b)
end

function join(r,delim,...)
    local out = ''
    local len = select('#',...)
    for i=1, len do
        if i > 1 then
            out = out .. (delim or ',')
        end
        out = out .. r[select(i,...)]
    end
    return out
end

function setbin(r,bin,val)
    r[bin] = val;
    aerospike:update(r);
    return r[bin];
end

function getbin(r,bin)
    return r[bin];
end

function cat(r,a,b,c,d,e,f)
    return (a or '') .. (b or '') .. (c or '') .. (d or '') .. (e or '') .. (f or '')
end

function abc(r,a,b)
    info(r,a,b)
    return "abc"
end

function log(r,msg)
    info(msg)
    return 1
end

function one(r)
    return 1
end

local function f1(b,a)
    b = b or {}
    b["sum"] = (b["sum"] or 0) + a
    return b
end

local function f2(a,b)
    return (a or 0)+b
end

function stream(s)
    return s : reduce(f2)
end