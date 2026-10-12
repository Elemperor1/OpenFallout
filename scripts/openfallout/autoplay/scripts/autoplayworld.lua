-- The world side of autoplay.lua: logs the stage of the quests of the intro of Fallout 3 and New Vegas whenever one changes,
-- and quits when the intro is over or the time is up. Lines start with AUTOPLAY.
local core = require('openfallout.core')
local world = require('openfallout.world')

-- The quests of the two intros, by editor id, and the stage of the main quest at which the intro is over
local QUESTS = { 'CG00', 'CG01', 'CG02', 'CG03', 'CG04', 'CGTutorial', 'MQ01', 'VCG00', 'VCG01', 'VCG02', 'VCG03', 'VCG04',
    'VMQ01' }
local DONE = { MQ01 = 10, VMQ01 = 10 }
-- Real seconds after which the run ends whatever happens
local LIMIT = 600

local last = {}
local elapsed = 0
local started = core.getRealTime()

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            local real = core.getRealTime() - started
            for _, name in ipairs(QUESTS) do
                local quest = world.quests.find(name)
                if quest then
                    local state = string.format('stage=%d running=%s', quest.stage, tostring(quest.running))
                    if last[name] ~= state then
                        last[name] = state
                        print('AUTOPLAY', 'quest', name, state, string.format('game=%.1f real=%.1f', elapsed, real))
                    end
                    if DONE[name] and quest.stage >= DONE[name] then
                        print('AUTOPLAY', 'intro over', name)
                        core.quit()
                    end
                end
            end
            if real >= LIMIT then
                print('AUTOPLAY', 'time is up')
                core.quit()
            end
        end,
    },
}
