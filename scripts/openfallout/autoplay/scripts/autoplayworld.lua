-- The world side of autoplay.lua: logs the stage of the quests of the intro of Fallout 3 and New Vegas whenever one changes,
-- does what the intro waits for when a quest stands still at a stage listed in autoplaygates.lua (walks the player into a
-- trigger volume, or makes the player activate an object), and quits when the intro is over or the time is up. Lines start
-- with AUTOPLAY.
local core = require('openfallout.core')
local world = require('openfallout.world')
local config = require('scripts.autoplaygates')

-- The quests of the two intros, by editor id, and the stage of the main quest at which the intro is over
local QUESTS = { 'CG00', 'CG01', 'CG02', 'CG03', 'CG04', 'CGTutorial', 'MQ01', 'VCG00', 'VCG01', 'VCG02', 'VCG03', 'VCG04',
    'VMQ01' }
local DONE = { MQ01 = 10, VMQ01 = 10, CG04 = 200, VCG01 = 200 }
-- Real seconds after which the run ends whatever happens
local LIMIT = 2400

local last, changed, tries = {}, {}, {}
-- What stops the world, told when it changes: the tags that pause it (the screens do, with the tag ui)
local pausedNow = ''
local elapsed = 0
local started = core.getRealTime()
local lastGate = 0

-- Does what the gate says: the player goes to the middle of the reference (a trigger volume), or activates it, or the quest
-- is put at a stage (for what the engine cannot bring about yet: a fight)
local function useGate(index, gate, real)
    tries[index] = (tries[index] or 0) + 1
    lastGate = real
    local ok, err = pcall(function()
        if gate.how == 'setstage' then
            if not world.quests.find(gate.quest):setStage(gate.to) then error('not set') end
            return
        end
        local object = world.getObjectByFormId(core.getFormId(gate.file, gate.ref))
        local player = world.players[1]
        if gate.how == 'activate' then
            object:activateBy(player)
        else
            player:teleport(object.cell, object.position)
        end
    end)
    local target = gate.how == 'setstage' and string.format('to=%d', gate.to) or string.format('ref=%06X', gate.ref)
    print('AUTOPLAY', 'gate', gate.how, gate.quest, string.format('stage=%d %s try=%d', gate.stage, target, tries[index]),
        ok and 'ok' or tostring(err))
end

return {
    engineHandlers = {
        onUpdate = function(dt)
            elapsed = elapsed + dt
            local real = core.getRealTime() - started
            local tags = {}
            for tag in pairs(world.getPausedTags()) do table.insert(tags, tag) end
            table.sort(tags)
            local paused = string.format('paused=%s tags=%s', tostring(core.isWorldPaused()), table.concat(tags, ','))
            if paused ~= pausedNow then
                pausedNow = paused
                print('AUTOPLAY', 'world', paused, string.format('real=%.1f', real))
            end
            for _, name in ipairs(QUESTS) do
                local quest = world.quests.find(name)
                if quest then
                    local state = string.format('stage=%d running=%s', quest.stage, tostring(quest.running))
                    if last[name] ~= state then
                        last[name] = state
                        changed[name] = real
                        print('AUTOPLAY', 'quest', name, state, string.format('game=%.1f real=%.1f', elapsed, real))
                    end
                    if DONE[name] and quest.stage >= DONE[name] then
                        print('AUTOPLAY', 'intro over', name)
                        core.quit()
                    end
                end
            end
            -- a quest that stands still at the stage of a gate: the intro waits for the player
            for index, gate in ipairs(config.gates) do
                local quest = world.quests.find(gate.quest)
                if quest and quest.stage == gate.stage and (tries[index] or 0) < (gate.tries or config.tries)
                    and real - (changed[gate.quest] or real) >= (gate.settle or config.settle)
                    and real - lastGate >= config.settle then
                    useGate(index, gate, real)
                end
            end
            if real >= LIMIT then
                print('AUTOPLAY', 'time is up')
                core.quit()
            end
        end,
    },
}
