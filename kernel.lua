-- See Copyright Notice in LICENSE.txt

--======================
-- Wrap unsafe functions
--======================

do
    -- Rep can take too much memory/cpu
    local old_rep = string.rep
    string.rep = function(s, n)
        if n > 8192 then
            error("n too large")
        elseif n < 0 then
            error("n cannot be negative")
        end
        return old_rep(s, n)
    end
end

--=============
-- Sandboxing
--=============

function create_sandbox()
    local sandbox = {
        error = error;
        assert = assert;
        ipairs = ipairs;
        next = next;
        pairs = pairs;
        pcall = pcall;
        rawequal = rawequal;
        rawget = rawget;
        rawset = rawset;
        select = select;
        tonumber = tonumber;
        tostring = tostring;
        type = type;
        unpack = unpack;
        xpcall = xpcall;
        setmetatable = setmetatable;
        struct = {
            unpack = struct.unpack;
        };

        coroutine = {
            create = coroutine.create;
            resume = coroutine.resume;
            running = coroutine.running;
            status = coroutine.status;
            wrap = coroutine.wrap;
            yield = coroutine.yield;
        };

        debug = {
            traceback = function(message, level)
                local message = tostring(message or "")
                local level = tonumber(level) or 1
                assert(level >= 0, "level is negative")
                assert(level < 256, "level too large")
                return debug.traceback(message, level)
            end;
        };

        math = {
            abs = math.abs;
            acos = math.acos;
            asin = math.asin;
            atan = math.atan;
            atan2 = math.atan2;
            ceil = math.ceil;
            cos = math.cos;
            cosh = math.cosh;
            deg = math.deg;
            exp = math.exp;
            floor = math.floor;
            fmod = math.fmod;
            frexp= math.frexp;
            ldexp = math.ldexp;
            log = math.log;
            log10 = math.log10;
            max = math.max;
            min = math.min;
            modf = math.modf;
            pi = math.pi;
            pow = math.pow;
            rad = math.rad;
            sin = math.sin;
            sinh = math.sinh;
            sqrt = math.sqrt;
            tan = math.tan;
            tanh = math.tanh;
            random = math.random;
            randomseed = math.randomseed;
        };

        string = {
            byte = string.byte;
            char = string.char;
            find = string.find;
            format = string.format;
            gmatch = string.gmatch;
            gsub = string.gsub;
            len = string.len;
            lower = string.lower;
            match = string.match;
            rep = string.rep;
            reverse = string.reverse;
            sub = string.sub;
            upper = string.upper;
        };

        table = {
            insert = table.insert;
            concat = table.concat;
            maxn = table.maxn;
            remove = table.remove;
            sort = table.sort;
        };

        print = print;

        loadstring = function(code, chunkname)
            if string.byte(code, 1) == 27 then
                error("no precompiled code")
            else
                return setfenv(assert(loadstring(code, chunkname)), sandbox)
            end
        end;

        resource = {
            render_child = render_child;
            load_image = load_image;
            load_video = load_video;
            load_font = load_font;
            load_file = load_file;
            create_shader = create_shader;
        };

        gl = {
            setup = function(width, height)
                setup(width, height)
                sandbox.WIDTH = width
                sandbox.HEIGHT = height
            end;
            clear = clear;
            pushMatrix = glPushMatrix;
            popMatrix = glPopMatrix;
            rotate = glRotate;
            translate = glTranslate;
        };

        sys = {
            now = now;
            list_childs = list_childs;
            send_child = send_child;
        };

        event = {
            content_update = function(name) 
            end;

            content_remove = function(name)
            end;

            render = function()
            end;

            raw_data = function(data, is_osc, suffix)
                if is_osc then
                    if string.byte(data, 1, 1) ~= 44 then
                        print("no osc type tag string")
                        return
                    end
                    local typetags, offset = struct.unpack(">!4s", data)
                    local tags = {string.byte(typetags, 1, offset)}
                    local fmt = ">!4"
                    for idx, tag in ipairs(tags) do
                        if tag == 44 then -- ,
                            fmt = fmt .. "s"
                        elseif tag == 105 then -- i
                            fmt = fmt .. "i4"
                        elseif tag == 102 then -- f
                            fmt = fmt .. "f"
                        elseif tag == 98 then -- b
                            print("no blob support")
                            return
                        else
                            print("unknown type tag " .. string.char(tag))
                            return
                        end
                    end
                    local unpacked = {struct.unpack(fmt, data)}
                    table.remove(unpacked, 1) -- remove typetags
                    table.remove(unpacked, #unpacked) -- remove trailing offset
                    return sandbox.event.osc(suffix, unpack(unpacked))
                else
                    return sandbox.event.data(data, suffix)
                end
            end;

            data = function(...)
                print(PATH, "data", ...)
            end;

            osc = function(...)
                print(PATH, "osc", ...)
            end;

            msg = function(...)
                print(PATH, "msg", ...)
            end;
        };

        NAME = NAME;
        PATH = PATH;
    }
    sandbox._G = sandbox
    return sandbox
end

function load_into_sandbox(code, chunkname)
    setfenv(
        assert(loadstring(code, chunkname)),
        sandbox
    )()
end

NODE_CODE_FILE = "node.lua"

function reload(usercode_file)
    sandbox = create_sandbox()

    -- load userlib
    load_into_sandbox(USERLIB, "userlib.lua")

    if usercode_file then
        local node_code = load_file(usercode_file)
        load_into_sandbox(node_code, "=" .. PATH .. "/" .. NODE_CODE_FILE)
    end
end

-- Einige Funktionen in der registry speichern, 
-- so dass der C Teil dran kommt.
do
    local registry = debug.getregistry()

    registry.traceback = debug.traceback

    registry.execute = function(cmd, ...)
        if cmd == "boot" then
            print "booting node"
            reload(NODE_CODE_FILE)
        elseif cmd == "event" then
            setfenv(
                function(event_name, ...)
                    event[event_name](...)
                end,
                sandbox
            )(...)
        elseif cmd == "update_content" then
            local name, added = ...
            if name == NODE_CODE_FILE then
                if added then
                    print "updating node code"
                    reload(NODE_CODE_FILE)
                else
                    print "removing node code"
                    reload()
                end
            else
                if added then
                    print("content updated: " .. name)
                    sandbox.event.content_update(name)
                else
                    print("content removed: " .. name)
                    sandbox.event.content_remove(name)
                end
            end
        elseif cmd == "render_self" then
            local screen_width, screen_height = ...
            local self = render_self()
            local root_width, root_height = self:size()
            local x1, y1, x2, y2 = sandbox.util.scale_into(
                root_width, root_height, screen_width, screen_height
            )
            self:draw(x1, y1, x2, y2)
        end
    end

    registry.alarm = function()
        error("CPU usage too high")
    end
end

io = nil
require = nil
loadfile = nil
load = nil
package = nil
module = nil
os = nil
dofile = nil
getfenv = nil
debug = {
    traceback = debug.traceback;
    getinfo = debug.getinfo;
}
