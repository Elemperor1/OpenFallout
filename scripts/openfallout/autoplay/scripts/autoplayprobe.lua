-- The probe of the saves (M7), a world script that is loaded beside autoplayworld.lua by autoplaysaves.omwscripts. Where the
-- table `saves` of autoplaygates.lua says (`save`: a quest has stood still at its stage for `after` real seconds), the game is
-- saved with the description `autoplay-save`; the quests (stage and whether they run) are written into the save and logged
-- ("AUTOPLAY digest saved"). Where `load` says the same, the game is loaded back in the same engine, once. A game that was
-- loaded from the save, in this engine or in a new one (--load-savegame), logs the quests as it has them as soon as it runs
-- ("digest loaded"), says whether they are those of the save ("digest check ok" or "MISMATCH") and saves again a few seconds
-- later with the description `autoplay-again`, so that the two files can be compared (scripts/openfallout/savediff.py). The
-- phase is kept in the save: 'play' until the game is saved, 'saved' in that save, and 'resaved' in the second. The saving and
-- the loading are done by autoplaysaves.lua, a menu script. Lines start with AUTOPLAY.
local core = require('openfallout.core')
local types = require('openfallout.types')
local world = require('openfallout.world')
local config = require('scripts.autoplaygates')

local saves = config.saves or {}
-- The quests that make the digest: those of the two intros, and any other that a gate waits for
local QUESTS = { 'CG00', 'CG01', 'CG02', 'CG03', 'CG04', 'CGTutorial', 'MQ01', 'VCG00', 'VCG01', 'VCG02', 'VCG03', 'VCG04',
    'VMQ01' }
local watched, known = {}, {}
local function watch(name)
    if not known[name] then
        known[name] = true
        table.insert(watched, name)
    end
end
for _, name in ipairs(QUESTS) do watch(name) end
for _, gate in ipairs(config.gates) do watch(gate.quest) end
for _, list in ipairs({ saves.save or {}, saves.load or {} }) do
    for _, want in ipairs(list) do watch(want.quest) end
end

local phase, savedDigest = 'play', nil
local started = core.getRealTime()
-- real seconds since the stage of each quest changed
local stage, since = {}, {}
local loadedAt, checked, askedResave = nil, false, false

local function digest()
    local parts = {}
    for _, name in ipairs(watched) do
        local quest = world.quests.find(name)
        if quest then table.insert(parts, string.format('%s=%d/%s', name, quest.stage, quest.running and 'r' or '-')) end
    end
    return table.concat(parts, ' ')
end

local function askMenu(event, data)
    types.Player.sendMenuEvent(world.players[1], event, data)
end

-- Whether one of the quests of the list has stood at its stage long enough
local function stoodAt(wants, real)
    for _, want in ipairs(wants or {}) do
        local quest = world.quests.find(want.quest)
        if quest and quest.stage == want.stage and real - (since[want.quest] or real) >= (saves.after or 5) then
            return true
        end
    end
    return false
end

return {
    engineHandlers = {
        onSave = function()
            return { phase = phase, digest = savedDigest }
        end,
        onLoad = function(data)
            if data == nil then return end
            phase, savedDigest = data.phase or 'play', data.digest
            print('AUTOPLAY', 'loaded', 'phase=' .. phase)
            if phase == 'saved' then loadedAt = core.getRealTime() - started end
        end,
        onUpdate = function()
            local real = core.getRealTime() - started
            for _, name in ipairs(watched) do
                local quest = world.quests.find(name)
                if quest and stage[name] ~= quest.stage then
                    stage[name] = quest.stage
                    since[name] = real
                end
            end
            if loadedAt ~= nil then
                -- the game was loaded from the save: the first frame tells what the quests are, a few seconds later it is
                -- saved again
                if not checked then
                    checked = true
                    local now = digest()
                    print('AUTOPLAY', 'digest', 'loaded', now)
                    print('AUTOPLAY', 'digest', now == savedDigest and 'check ok' or 'check MISMATCH')
                end
                if not askedResave and real - loadedAt >= (saves.resaveAfter or 3) then
                    askedResave = true
                    phase = 'resaved'
                    askMenu('AutoplaySave', { description = 'autoplay-again' })
                end
            elseif phase == 'play' and stoodAt(saves.save, real) then
                phase, savedDigest = 'saved', digest()
                print('AUTOPLAY', 'digest', 'saved', savedDigest)
                askMenu('AutoplaySave', { description = 'autoplay-save' })
            elseif phase == 'saved' and stoodAt(saves.load, real) then
                phase = 'loading'
                askMenu('AutoplayLoad', { description = 'autoplay-save' })
            end
        end,
    },
}
