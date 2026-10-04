local util = require('openfallout.util')
local core = require('openfallout.core')
local self = require('openfallout.self')

local player = nil

local function printToConsole(...)
    local strs = {}
    for i = 1, select('#', ...) do
        strs[i] = tostring(select(i, ...))
    end
    player:sendEvent('OFConsolePrint', table.concat(strs, '\t'))
end

local function printRes(...)
    if select('#', ...) >= 0 then
        printToConsole(...)
    end
end

local env = {
    I = require('openfallout.interfaces'),
    util = require('openfallout.util'),
    storage = require('openfallout.storage'),
    core = require('openfallout.core'),
    types = require('openfallout.types'),
    vfs = require('openfallout.vfs'),
    markup = require('openfallout.markup'),
    async = require('openfallout.async'),
    nearby = require('openfallout.nearby'),
    self = require('openfallout.self'),
    aux_util = require('openfallout_aux.util'),
    anim = require('openfallout.animation'),
    calendar = require('openfallout_aux.calendar'),
    time = require('openfallout_aux.time'),
    view = require('openfallout_aux.util').deepToString,
    print = printToConsole,
    exit = function() player:sendEvent('OFConsoleExit') end,
    help = function() player:sendEvent('OFConsoleHelp') end,
}
env._G = env
setmetatable(env, {__index = _G, __metatable = false})
_G = nil

local function executeLuaCode(code)
    local fn
    local ok, err = pcall(function() fn = util.loadCode('return ' .. code, env) end)
    if ok then
        ok, err = pcall(function() printRes(fn()) end)
    else
        ok, err = pcall(function() util.loadCode(code, env)() end)
    end
    if not ok then
        player:sendEvent('OFConsoleError', err)
    end
end

return {
    eventHandlers = {
        OFConsoleEval = function(data)
            player = data.player
            env.selected = data.selected
            executeLuaCode(data.code)
            if env.selected ~= data.selected then
                local ok, err = pcall(function() player:sendEvent('OFConsoleSetSelected', env.selected) end)
                if not ok then player:sendEvent('OFConsoleError', err) end
            end
        end,
    },
    engineHandlers = {
        onLoad = function()
            core.sendGlobalEvent('OFConsoleStopLocal', self.object)
        end,
    }
}

