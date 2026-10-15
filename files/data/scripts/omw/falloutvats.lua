-- V.A.T.S. in Fallout 3 and New Vegas: the V key stops the game, the player picks a target in view and the parts of its
-- body to attack, each attack costs action points, and the queue is carried out when the screen is left with Attack.
-- Which parts a body has, the chance to hit each and the attack itself are the engine's (core.vats); this script lists
-- them and gives the choices of the player back. There is no slow motion and no camera yet: the attacks come one after
-- the other, a moment apart.
local core = require('openfallout.core')
local camera = require('openfallout.camera')
local input = require('openfallout.input')
local nearby = require('openfallout.nearby')
local self = require('openfallout.self')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local Player = require('openfallout.types').Player
local widgetModule = require('scripts.omw.falloutwidgets')

local vats = core.vats

if not vats.available() then
    return {}
end

local v2, v3 = util.vector2, util.vector3
local colors = widgetModule
local textSize = widgetModule.textSize

-- How many targets the screen offers, how far from the middle of the view they may be (the cosine of the angle), how high
-- on a character the line of sight ends, and the seconds between two attacks of a queue
local maxTargets = 6
local viewCosine = 0.5
local headHeight = 70
local attackDelay = 0.7

local element = nil
local modeOpen = false
local widgets
-- The characters on offer, the one that is picked, and the attacks that were queued: {target, part, name, chance}
local targets = {}
local picked = 1
local queue = {}
-- The queue that is being carried out, and the seconds until its next attack
local running = nil
local wait = 0

local render

local function update()
    if element then element:update() end
end

local function controlsAllowed()
    return Player.getControlSwitch(self, Player.CONTROL_SWITCH.Controls)
        and Player.getControlSwitch(self, Player.CONTROL_SWITCH.Fighting)
end

-- The characters in view that the weapon of the player reaches: nearest to the middle of the view first
local function inView()
    local found = {}
    local from = camera.getPosition()
    local forward = camera.viewportToWorldVector(v2(0.5, 0.5))
    for _, actor in ipairs(nearby.actors) do
        if actor:isValid() and vats.canTarget(actor) then
            local head = actor.position + v3(0, 0, headHeight)
            local direction = (head - from):normalize()
            local cosine = direction:dot(forward)
            if cosine >= viewCosine then
                local ray = nearby.castRay(from, head, { ignore = self, collisionType = nearby.COLLISION_TYPE.Default })
                if ray.hitObject == nil or ray.hitObject == actor then
                    found[#found + 1] = { actor = actor, cosine = cosine }
                end
            end
        end
    end
    table.sort(found, function(a, b) return a.cosine > b.cosine end)
    local actors = {}
    for index = 1, math.min(#found, maxTargets) do
        actors[index] = found[index].actor
    end
    return actors
end

local function spent()
    return #queue * vats.state().cost
end

-- The screen is gone: nothing is drawn, and the engine hears that V.A.T.S. is closed
local function finish()
    modeOpen = false
    if element then
        element:destroy()
        element = nil
    end
    vats.setOpen(false)
end

local function closeMode()
    if modeOpen then
        finish()
        I.UI.removeMode('Interface')
    end
end

-- Carries out the queue that was made
local function go()
    if #queue == 0 then
        return
    end
    running = queue
    queue = {}
    wait = 0
    closeMode()
end

function render()
    if element then
        element:destroy()
        element = nil
    end
    if #targets == 0 then
        return
    end
    widgets = widgetModule.new(update)
    local state = vats.state()
    local width = math.max(520, math.floor(ui.screenSize().x * 0.4))
    local rows = { { props = { size = v2(width, 0) } } }
    local function add(layout)
        rows[#rows + 1] = { props = { size = widgetModule.gap } }
        rows[#rows + 1] = layout
    end
    local left = state.actionPoints - spent()
    add(widgets.label('V.A.T.S.', colors.titleColor, textSize + 4))
    add(widgets.label(string.format('Action points %d / %d   %s costs %d', math.floor(left + 0.5),
        math.floor(state.maxActionPoints + 0.5), state.weapon ~= '' and state.weapon or 'Fists',
        math.floor(state.cost + 0.5)), colors.normalColor, textSize - 2))

    -- The targets: the one that is picked has its parts below
    local cells = {}
    for index, actor in ipairs(targets) do
        local info = vats.describe(actor)
        local label = string.format('%s %d%%', info.name ~= '' and info.name or 'Target',
            info.maxHealth > 0 and math.floor(100 * info.health / info.maxHealth + 0.5) or 0)
        cells[#cells + 1] = widgets.cell(widgets.clickable(label, function()
            picked = index
            render()
        end, { color = index == picked and colors.pickedColor or colors.normalColor }), 190)
    end
    add(widgets.row(table.unpack(cells)))

    local target = targets[picked]
    local parts = target and vats.parts(target) or {}
    if #parts == 0 then
        add(widgets.label('Nothing to aim at', colors.hintColor))
    end
    for _, part in ipairs(parts) do
        local crippled = part.condition <= 0
        local text = string.format('%s  %d%%%s', part.target, math.floor(part.chance + 0.5), crippled and '  crippled' or '')
        add(widgets.clickable(text, function()
            queue[#queue + 1] = { target = target, part = part.index, name = part.target, chance = part.chance }
            render()
        end, { enabled = left >= state.cost and part.chance > 0, color = colors.normalColor }))
    end

    if #queue > 0 then
        add(widgets.label('Queued', colors.hintColor, textSize - 4))
        for index, attack in ipairs(queue) do
            add(widgets.label(string.format('%d. %s %d%%', index, attack.name, math.floor(attack.chance + 0.5)),
                colors.normalColor, textSize - 2))
        end
    end
    add(widgets.row(
        widgets.cell(widgets.clickable('Attack', go, { enabled = #queue > 0, color = colors.titleColor }), 120),
        widgets.cell(widgets.clickable('Undo', function()
            queue[#queue] = nil
            render()
        end, { enabled = #queue > 0, color = colors.titleColor }), 100),
        widgets.clickable('Cancel', function()
            queue = {}
            closeMode()
        end, { color = colors.titleColor })))
    element = ui.create(widgets.panel(rows))
end

-- Opens the screen. `offered` is a list of characters to offer in place of those in view (for scripts and tests).
local function open(offered)
    if modeOpen or running or I.UI.getMode() ~= nil then
        return
    end
    if not vats.state().usable then
        return
    end
    targets = offered or inView()
    if #targets == 0 then
        ui.showMessage('No target in sight')
        return
    end
    picked, queue = 1, {}
    modeOpen = true
    I.UI.addMode('Interface', { windows = {} })
    vats.setOpen(true)
    render()
end

local function cycle(by)
    if element and #targets > 1 then
        picked = (picked - 1 + by) % #targets + 1
        render()
    end
end

-- The next attack of the queue that is being carried out
local function step()
    local attack = table.remove(running, 1)
    local result = vats.attack(attack.target, attack.part)
    if result.refused then
        if result.refused == 'points' then
            ui.showMessage('Not enough action points')
        elseif result.refused == 'ammo' then
            ui.showMessage('Out of ammunition')
        end
    elseif result.hit then
        ui.showMessage(string.format('%s%s%s', attack.name, result.crippled and ' crippled' or ' hit',
            result.killed and ', dead' or ''))
    else
        ui.showMessage(attack.name .. ' missed')
    end
    -- A queue ends when nothing is left of it or the player can not go on
    if result.refused == 'points' or result.refused == 'ammo' or #running == 0 then
        running = nil
    end
end

return {
    engineHandlers = {
        onKeyPress = function(key)
            if key.code == input.KEY.V and not modeOpen and controlsAllowed() then
                open()
            elseif not element then
                return
            elseif key.code == input.KEY.Enter then
                go()
            elseif key.code == input.KEY.RightArrow or key.code == input.KEY.Tab then
                cycle(1)
            elseif key.code == input.KEY.LeftArrow then
                cycle(-1)
            end
        end,
        -- The attacks of a queue come a moment apart while the game runs
        onUpdate = function(dt)
            if not running then
                return
            end
            wait = wait - dt
            if wait <= 0 then
                wait = attackDelay
                step()
            end
        end,
    },
    eventHandlers = {
        OpenVats = function(data)
            open(data and data.targets)
        end,
        -- However the screen is left (Escape, a script), V.A.T.S. is closed and what was queued is dropped
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                finish()
                queue = {}
            end
        end,
    },
}
