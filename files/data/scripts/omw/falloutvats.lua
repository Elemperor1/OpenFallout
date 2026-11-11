-- V.A.T.S. in Fallout 3 and New Vegas: the V key stops the game, the player picks a target in view and the parts of its
-- body to attack, each attack costs action points, and the queue is carried out when the screen is left with Accept.
-- Which parts a body has, the chance to hit each and the attack itself are the engine's (core.vats); this script draws the
-- games' own file (vats_menu.xml) the way FalloutNV.exe does (Mac rounds 12BH and 12BI, VATSMenu: create 0x7E9200, click
-- dispatch 0x7EECB0, layout 0x7F1840) and gives the choices of the player back:
--   * the three blocks the file leaves empty are made by the code: the health (a bar and the word), the action points (two
--     bars, the one in front what is left when the queued attacks are paid, the half-lit one behind it what the next
--     attack would take; the word, the rounds, and a message when the points do not reach) and the enemy (its bracket, its
--     health bar and its name). The bars are rows of whole ticks, as the HUD's;
--   * a bracket (the template body_part_percent) is made for every part of the picked target's body, at the place the
--     part's bone has on the screen, 100 to the side (the head 100 up or down, the torso on it), kept inside a box 130 from
--     the edges, and pushed apart when two are closer than 150 by 100 (up to 30 passes). Its picture is the file's by the
--     side of the screen the bracket is on, it shows the chance (at most 95), the condition of the part as a bar and, for the
--     part that is picked out, the name of the part;
--   * a click where nothing takes it (the attack control) queues an attack on the picked part, A and D (or the arrows of the
--     file's two arrows) pick the target, W and S (or the wheel) the part, E (or Enter) accepts and leaves the screen to
--     carry the queue out, B or the right button takes the last attack off the queue and leaves the screen when there is none;
--   * the queue is the file's list, a row for each attack ("<name>: <part>").
-- Not done: the camera of V.A.T.S. (the view is the player's own), slow motion, the special attacks, the condition of the
-- weapon, the compass, the rows for a reload, and the name of the target with its health and threshold (a perk). The attacks
-- of a queue come one after the other, a moment apart. Where the file cannot be shown the older screen of Morrowind's
-- widgets takes its place.
local core = require('openfallout.core')
local camera = require('openfallout.camera')
local input = require('openfallout.input')
local nearby = require('openfallout.nearby')
local self = require('openfallout.self')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

-- A message box drawn over the screen (falloutmessage.lua) has the keys, the pad and the wheel
local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end
local Player = require('openfallout.types').Player
local widgetModule = require('scripts.omw.falloutwidgets')
local menuModule = require('scripts.omw.falloutmenu')
local pad = require('scripts.omw.falloutpad')

-- A note to the player goes to the line of messages of the display, as the games print it, while the display is the
-- game's own file (the engine's own message window is the fallback)
local function tellPlayer(text)
    local hud = I.FalloutHud
    if hud and hud.note then
        hud.note(text)
    else
        ui.showMessage(text)
    end
end

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

local FILE = 'vats_menu.xml'
-- The menu is 960 units high; the safe zone is 15 on each side (the file says 2 x that for the blocks)
local HEIGHT, SAFE = 960, 15
-- The seconds the file's fade takes in and out (its explorefade)
local FADE_SECONDS = 0.25
-- The justification of a bracket in the file: left, right, up and down (up is the head, down the chest)
local LEFT, RIGHT, UP, DOWN = 1, 4, 5, 6
-- How far under the top of its row the text of a row of the queue is (the file's y of ListItemText)
local QUEUE_TEXT_Y = 20
-- The most the chance on a bracket shows (fVATSMaxChance)
local MAX_SHOWN = 95

local element = nil
local modeOpen = false
local widgets
-- The characters on offer, the one that is picked, and the attacks that were queued: {target, part, name, chance}
local targets = {}
local picked = 1
local queue = {}
-- The part of the picked one that is picked out (its place in the list of parts); on the widgets of the older screen it is
-- drawn once a key has moved it
local partPicked, partFocus = 1, false
-- The queue that is being carried out, and the seconds until its next attack
local running = nil
local wait = 0

-- The screen made from the file: the menu that is shown, the tiles the code made, the parts of the picked target, and a
-- screen that was left and fades out
local screen, tiles = nil, nil
local fading = nil
local cache = {}
local shownParts = {}
local limbState = {}
local fadeAge = 0

local renderWidgets

local function update()
    if element then element:update() end
end

local function controlsAllowed()
    return Player.getControlSwitch(self, Player.CONTROL_SWITCH.Controls)
        and Player.getControlSwitch(self, Player.CONTROL_SWITCH.Fighting)
end

local function word(name, fallback)
    local ok, text = pcall(core.pipboy.text, name)
    if ok and type(text) == 'string' and text ~= '' then return text end
    return fallback or name
end

local function sound(name)
    if core.pipboy and core.pipboy.playSoundNamed then
        pcall(core.pipboy.playSoundNamed, name)
    end
end

local function padInUse()
    local ok, menu = pcall(require, 'openfallout.menu')
    if not ok or type(menu) ~= 'table' or menu.controllerConnected == nil then return false end
    local good, connected = pcall(menu.controllerConnected)
    return good and connected == true
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

-- The part a bracket stands for is one of the body part slots of the target (its type, 0 to 14), and the parts are picked
-- out one at a time in the order of the target's body part data (the executable's loop at 0x7F57F7 and its keys at
-- 0x7F3070). The group of a part is the actor value of its condition, which parts of one body can share (the three arm
-- parts of a Handy are all 28): the parts of the group of the picked one are shown together. 26 is the torso's.
local TORSO_GROUP = 26
-- Where a part is taken to be when its bone is not in the model: how high above the feet of a character, by its type
local FALLBACK_HEIGHT = {
    [0] = 100, [1] = 150, [2] = 150, [13] = 150, [3] = 105, [4] = 105, [5] = 105, [6] = 105, [7] = 40, [8] = 40, [9] = 40,
    [10] = 40, [11] = 40, [12] = 40, [14] = 90,
}

-- The slot a part stands in is its type (the body part type of the part record, 0 torso to 14 weapon); the index of a
-- part is its place in the list of parts of the body, which is only the number an attack names it by
local function slotOf(part)
    return part.type or part.index
end

-- The key of the group of a part: its condition's actor value, or the part alone when it has none
local function groupOf(part)
    if part.group ~= nil and part.group >= 0 then return part.group end
    return 'part' .. tostring(part.index)
end

local function partName(part)
    return part.name ~= '' and part.name or part.target
end

-- ----------------------------------------------------------------------------------------------------------------------
-- The screen made from the file

local function put(tile, trait, value)
    local key = tile .. '|' .. trait
    if cache[key] == value then return end
    cache[key] = value
    screen.menu:set(tile, trait, value)
end

-- The width of a bar of whole ticks, as the HUD makes its bars (0x7748B0)
local function barWidth(total, tick, share, least)
    share = math.max(0, math.min(1, share))
    local width = math.floor(total * share / tick + 0.5) * tick
    return math.max(width, tick * least)
end

-- How wide the menu is (it is 960 high, and as wide as the screen's shape makes it)
local function menuWidth(size)
    return size.y > size.x and 1280 or math.floor(size.x / size.y * HEIGHT)
end

-- The three blocks: health bottom left, action points bottom right, the enemy between them
local function placeBlocks(m, t)
    local hpY = HEIGHT - 2 * SAFE - 127
    local function place(tile, x, y)
        m:set(tile, 'x', x)
        m:set(tile, 'y', y)
    end
    place(t.hitPoints, 2 * SAFE + 10, hpY)
    place(t.actionPoints, t.width - 2 * SAFE - 386 + 30, hpY)
    place(t.enemy, math.floor(t.width / 2) - 175, HEIGHT - 2 * SAFE - 88)
end

local function buildScreen()
    -- Opening can raise as well as return nil (the pictures are made by the interface module): either way the older
    -- screen takes over, as the mode and the pause were already set when this is called
    local good, opened = pcall(menuModule.open, FILE, { layer = 'Windows', pad = padInUse() })
    if not good then
        print('V.A.T.S. is not drawn from its file: ' .. tostring(opened))
        return false
    end
    if not opened then return false end
    local ok, problem = pcall(function()
        local m = opened.menu
        local size = ui.screenSize()
        local t = { width = menuWidth(size), size = size }
        local function find(name)
            local id = m:find(name)
            if not id then error('the file has no tile ' .. name, 0) end
            return id
        end
        local function make(parent, template, name, traits)
            local tile = opened:instantiate(parent, template, name)
            for trait, value in pairs(traits or {}) do m:set(tile, trait, value) end
            return tile
        end
        t.root = m:root()
        t.glow = find('GlowBranch')
        -- The special attacks are shown when the weapon has one (the code asks for its cost); they are not made here
        for _, name in ipairs({ 'special_attack_button', 'special_attack_2_button' }) do
            local id = m:find(name)
            if id then m:set(id, 'visible', 0) end
        end
        t.accept = find('accept_button')
        m:set(t.accept, 'target', 0)

        local hitPoints, actionPoints, enemy = find('HitPoints'), find('ActionPoints'), find('EnemyHealth')
        t.hitPoints, t.actionPoints, t.enemy = hitPoints, actionPoints, enemy
        placeBlocks(m, t)

        make(hitPoints, 'template_left_bracket', 'vats_hp_bracket', { brightness = 255 })
        t.hpMeter = make(hitPoints, 'template_meter', 'vats_hp_meter', { _OriginalX = 20, x = 20, y = 41, brightness = 175 })
        make(hitPoints, 'template_justify_right_text', 'vats_hp_text', { x = 53, y = 8, string = word('sHitPointsShort', 'HP') })

        make(actionPoints, 'template_right_bracket', 'vats_ap_bracket', { x = -37, brightness = 255 })
        -- The bar behind is the half-lit one: what the next attack would use
        t.apBefore = make(actionPoints, 'template_meter', 'vats_ap_before', { _OriginalX = 326, y = 41, depth = 5, brightness = 175, alpha = 128 })
        t.apAfter = make(actionPoints, 'template_meter', 'vats_ap_after', { _OriginalX = 326, y = 41, depth = 5, brightness = 175 })
        make(actionPoints, 'template_justify_right_text', 'vats_ap_text',
            { x = 315, y = 8, string = word('sActionPointsShort', 'AP') })
        t.apLow = make(actionPoints, 'template_justify_right_text', 'vats_ap_low',
            { x = 270, y = 8, visible = 0, string = word('sVATSMessageLowAP', 'Not enough AP') })
        t.ammo = make(actionPoints, 'template_justify_right_text', 'vats_ammo', { x = 315, y = 80, font = 7, visible = 0 })

        make(enemy, 'template_enemy_health_bracket', 'vats_enemy_bracket', { brightness = 175, visible = 1 })
        t.enemyMeter = make(enemy, 'template_meter', 'vats_enemy_meter',
            { _OriginalX = 171, _TotalWidth = 240, y = 3, visible = 1, brightness = 175 })
        t.enemyName = make(enemy, 'template_justify_center_text', 'vats_enemy_name', { x = 170, y = 35, wrapwidth = 350 })
        m:set(enemy, 'systemcolor', 2)

        t.tick = { hp = m:getNumber(t.hpMeter, '_ImageWidth'), ap = m:getNumber(t.apAfter, '_ImageWidth'),
            enemy = m:getNumber(t.enemyMeter, '_ImageWidth') }
        t.total = { hp = m:getNumber(t.hpMeter, '_TotalWidth'), ap = m:getNumber(t.apAfter, '_TotalWidth'),
            enemy = m:getNumber(t.enemyMeter, '_TotalWidth') }

        -- A bracket for each of the 15 slots a body has parts in
        t.limb = {}
        for slot = 0, 14 do
            t.limb[slot] = make(t.glow, 'body_part_percent', 'vats_limb' .. slot, { alpha = 0 })
        end
        local list = find('queued_actions')
        t.list = opened:list('queued_actions', 'queued_action')
        t.listVisible = m:getNumber(list, '_number_of_visible_items')
        local line = m:measure(7, 'Ag', 2048)
        t.lineHeight = line and line.height or 36
        screen, tiles = opened, t
    end)
    if not ok then
        print('V.A.T.S. is not drawn from its file: ' .. tostring(problem))
        opened:close()
        return false
    end
    return true
end

-- Where a point of the world is on the screen in the menu's units (nil when it is behind the viewer). It is worked out from
-- the view transform and the field of view and not from camera.worldToViewportVector, whose places are pixels of the
-- video resolution while the menu's size is that of the interface (the window divided by its scale): Mac round 12BJ
-- measured 1920x1080 against 960x540 and the brackets off the screen. The view space has x to the right, y up and the
-- viewer looking down -z.
local function project(position)
    local view = camera.getViewTransform():apply(position)
    local depth = -view.z
    if depth <= 0 then return nil end
    local half = math.tan(camera.getFieldOfView() / 2)
    local aspect = tiles.size.x / tiles.size.y
    return (0.5 + 0.5 * view.x / (depth * half * aspect)) * tiles.width, (0.5 - 0.5 * view.y / (depth * half)) * HEIGHT
end

-- Where the bracket of a part goes: the executable's offsets by part (0x7F2874), the box it is kept in and the passes that
-- push two brackets that are too close apart (0x7F1840). Returns, for each slot, the place and the justification.
local function layout(target, parts)
    local width = tiles.width
    local items = {}
    for _, part in ipairs(parts) do
        local x, y
        local position = vats.partPosition(target, part.target)
        if position == nil then
            local base = target.position
            position = v3(base.x, base.y, base.z + (FALLBACK_HEIGHT[slotOf(part)] or 100))
        end
        x, y = project(position)
        local item = { slot = slotOf(part), onScreen = false, x = 0, y = 0 }
        if x ~= nil and x > 0 and x < width and y > 0 and y < HEIGHT then
            item.onScreen = true
            local slot = slotOf(part)
            if slot == 1 or slot == 2 then
                y = y > HEIGHT / 2 and y + 100 or y - 100
            elseif slot ~= 0 and slot ~= 13 then
                x = x > width / 2 and x + 100 or x - 100
                y = y > HEIGHT / 2 and y + 50 or y - 50
            end
            item.x, item.y = x, y
            -- The side of the screen decides which picture an arm, a leg or a weapon takes
            if slot == 0 then item.justify = DOWN
            elseif slot == 1 or slot == 2 or slot == 13 then item.justify = UP
            else item.justify = x < width / 2 and RIGHT or LEFT end
            items[#items + 1] = item
        end
        part.item = item
    end
    local minX, minY = 2 * SAFE + 100, 2 * SAFE + 100
    local maxX, maxY = width - (2 * SAFE + 100) - 50, HEIGHT - 2 * SAFE - 256 + 50
    local function clamp(item)
        item.x = math.max(minX, math.min(maxX, item.x))
        item.y = math.max(minY, math.min(maxY, item.y))
    end
    for _, item in ipairs(items) do clamp(item) end
    for _ = 1, 30 do
        local moved = false
        for i = 1, #items - 1 do
            for j = i + 1, #items do
                local a, b = items[i], items[j]
                local dx, dy = b.x - a.x, b.y - a.y
                if math.abs(dx) < 150 and math.abs(dy) < 100 then
                    local overlapX, overlapY = 150 - math.abs(dx), 100 - math.abs(dy)
                    -- It is pushed along the axis it overlaps least (the half each way)
                    if overlapY / 100 <= overlapX / 150 then
                        local sign = dy >= 0 and 1 or -1
                        a.y, b.y = a.y - sign * overlapY / 2, b.y + sign * overlapY / 2
                    else
                        local sign = dx >= 0 and 1 or -1
                        a.x, b.x = a.x - sign * overlapX / 2, b.x + sign * overlapX / 2
                    end
                    clamp(a)
                    clamp(b)
                    moved = true
                end
            end
        end
        if not moved then break end
    end
end

-- A value that moves to another over some seconds, from where it is
local function tween(state, to, seconds, dt)
    if state.to ~= to or state.seconds ~= seconds then
        state.from, state.to, state.seconds, state.age = state.alpha, to, seconds, 0
    end
    state.age = state.age + dt
    state.alpha = state.from + (to - state.from) * math.min(1, state.age / seconds)
end

local function refreshParts()
    local target = targets[picked]
    shownParts = target and vats.parts(target) or {}
    partPicked = math.max(1, math.min(partPicked, math.max(1, #shownParts)))
end

-- Writes what the screen shows now (`dt` is the real seconds since the last frame)
local function updateScreen(dt)
    local t = tiles
    local state = vats.state()
    local stats = core.pipboy.stats()
    local target = targets[picked]
    screen:batch(function()
        -- The menu comes in over the file's fade time
        fadeAge = fadeAge + dt
        put(t.root, 'alpha', math.floor(255 * math.min(1, fadeAge / FADE_SECONDS) + 0.5))

        -- Health
        local share = stats.maxHealth > 0 and stats.health / stats.maxHealth or 0
        put(t.hpMeter, 'width', barWidth(t.total.hp, t.tick.hp, share, stats.health > 0 and 1 or 0))

        -- Action points: the queued attacks are paid already; the bar behind shows what the next attack would take
        local left = math.max(0, state.actionPoints - spent())
        local most = math.max(state.maxActionPoints, left, 1)
        local before, after
        if left >= state.cost then
            before, after = left / most, (left - state.cost) / most
        else
            before, after = 0, state.cost / most
        end
        local widthAfter = barWidth(t.total.ap, t.tick.ap, after, after > 0 and 1 or 0)
        local widthBefore = barWidth(t.total.ap, t.tick.ap, before, before > 0 and 1 or 0)
        put(t.apAfter, 'width', widthAfter)
        put(t.apAfter, 'x', 326 - widthAfter)
        put(t.apBefore, 'width', widthBefore)
        put(t.apBefore, 'x', 326 - widthBefore)
        put(t.apLow, 'visible', left < state.cost and 1 or 0)
        local weapon = core.combat and core.combat.weapon and core.combat.weapon() or nil
        local rounds = (weapon and (weapon.ammoUse or 0) > 0) and weapon.rounds or nil
        put(t.ammo, 'visible', rounds ~= nil and 1 or 0)
        put(t.ammo, 'string', rounds ~= nil and tostring(rounds) or '')

        -- The enemy: the bar grows from its middle
        if target then
            local info = vats.describe(target)
            local ratio = info.maxHealth > 0 and math.max(0, math.min(1, info.health / info.maxHealth)) or 0
            local width = barWidth(t.total.enemy, t.tick.enemy, ratio, ratio > 0 and 1 or 0)
            put(t.enemyMeter, 'width', width)
            put(t.enemyMeter, 'x', 171 - width / 2)
            put(t.enemyName, 'string', info.name)
        end

        -- The brackets
        local picked_part = shownParts[partPicked]
        local pickedGroup = picked_part and groupOf(picked_part) or nil
        if target and #shownParts > 0 then
            layout(target, shownParts)
        end
        local present = {}
        for _, part in ipairs(shownParts) do
            local slot = slotOf(part)
            local tile = t.limb[slot]
            local item = part.item
            if tile and item then
                present[slot] = true
                local st = limbState[slot]
                if not st then
                    st = { alpha = 0, to = 0, seconds = 1, age = 0, from = 0 }
                    limbState[slot] = st
                end
                local inPicked = groupOf(part) == pickedGroup
                if item.onScreen then
                    tween(st, 255, inPicked and 1.0 or 0.2, dt)
                    put(tile, 'x', math.floor(item.x + 0.5))
                    put(tile, 'y', math.floor(item.y + 0.5))
                    put(tile, 'justify', item.justify)
                else
                    tween(st, 0, 0.5, dt)
                end
                put(tile, 'alpha', math.floor(st.alpha + 0.5))
                put(tile, '_LimbName', partName(part))
                put(tile, '_LimbNameVisible', inPicked and 1 or 0)
                put(tile, '_MeterPercent', math.max(0, math.min(1, part.condition / 100)))
                local shownChance = math.max(0, math.min(MAX_SHOWN, math.floor(part.chance + 0.5)))
                put(tile, 'string', string.format('%d%%', part.chance > 0 and shownChance or 0))
            end
        end
        for slot = 0, 14 do
            if not present[slot] then
                put(t.limb[slot], 'alpha', 0)
                limbState[slot] = nil
            end
        end
    end)
end

-- Gives the queue list and the accept button what the queue is now
local function showQueue()
    local t = tiles
    local rows = {}
    -- The rows stand one over the other from the bottom of the list, the newest last. The text of a row (the file's
    -- ListItemText, right-justified at x 15) lies mostly to the left of the list's rectangle and is not cut off in the game,
    -- because the text has no `clips` of its own (Mac round 12BJ B1); the menu here cuts what is in a row that clips, so the
    -- row is set not to. The file's formula for a row's y puts the last row's top at the bottom of the list's rectangle and
    -- its text (20 under the top) over the action point text of the block below, which cannot be how the game looks; the
    -- formula counts the list's children, which this list has more of than the executable's, so the rows are placed by the
    -- port's rule instead: the last row's text ends at the bottom of the list's rectangle.
    for index, attack in ipairs(queue) do
        rows[index] = { string = attack.row, clips = 0,
            y = t.listVisible * t.lineHeight - (#queue - index + 1) * t.lineHeight - QUEUE_TEXT_Y }
    end
    if t.list then t.list.rows(rows) end
    put(t.accept, 'target', #queue > 0 and 1 or 0)
end

-- The screen is gone: it fades out for the file's seconds (and takes no pointer), and the engine hears that V.A.T.S. is
-- closed (unless the attacks of a queue are still to come, which are part of it)
local function finish(keepOpen)
    modeOpen = false
    if element then
        element:destroy()
        element = nil
    end
    if screen then
        if fading then fading.shown:close() end
        if screen.element then
            screen.element.layout.props.ignorePointerEvents = true
            screen.element:update()
        end
        fading = { shown = screen, left = FADE_SECONDS }
        screen, tiles = nil, nil
        cache, limbState, shownParts = {}, {}, {}
    end
    if not keepOpen then
        vats.setOpen(false)
    end
end

local function closeMode(keepOpen)
    if modeOpen then
        finish(keepOpen)
        I.UI.removeMode('Interface')
    end
end

-- The last attack of a queue is taken (or the player can not go on): V.A.T.S. is over
local function endQueue()
    running = nil
    vats.setOpen(false)
end

-- Carries out the queue that was made
local function go()
    if #queue == 0 then
        return
    end
    running = queue
    queue = {}
    wait = 0
    -- The attacks come after the screen is gone, and the action points do not come back until the last of them
    closeMode(true)
end

local function render()
    if screen then
        refreshParts()
        showQueue()
        updateScreen(0)
    else
        renderWidgets()
    end
end

-- Adds an attack on a part of the target to the queue
local function queueAttack(target, part, name)
    local info = vats.describe(target)
    queue[#queue + 1] = {
        target = target, part = part.index, name = name, chance = part.chance,
        row = string.format('%s: %s', info.name ~= '' and info.name or word('sTarget', 'Target'), name),
    }
    sound('UIVATSMove')
    print(string.format('V.A.T.S.: queued %s %d%%', name, math.floor(math.min(part.chance, 100) + 0.5)))
    render()
end

-- The older screen on Morrowind's widgets, for an install where the file of the game cannot be shown
function renderWidgets()
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

    -- The targets, two to a row so that a name and its health fit: the one that is picked has its parts below
    local cells = {}
    for index, actor in ipairs(targets) do
        local info = vats.describe(actor)
        local label = string.format('%s %d%%', info.name ~= '' and info.name or 'Target',
            info.maxHealth > 0 and math.floor(100 * info.health / info.maxHealth + 0.5) or 0)
        cells[#cells + 1] = widgets.cell(widgets.clickable(label, function()
            picked = index
            renderWidgets()
        end, { color = index == picked and colors.pickedColor or colors.normalColor }), math.floor(width / 2) - 8)
        if #cells == 2 or index == #targets then
            add(widgets.row(table.unpack(cells)))
            cells = {}
        end
    end

    local target = targets[picked]
    local parts = target and vats.parts(target) or {}
    if #parts == 0 then
        add(widgets.label('Nothing to aim at', colors.hintColor))
    end
    partPicked = math.max(1, math.min(partPicked, #parts))
    for index, part in ipairs(parts) do
        local crippled = part.condition <= 0
        -- A perk can take the chance over 100 (it is capped before the perks): a hit is sure then, and 100 is what shows
        local shown = math.min(part.chance, 100)
        -- The name of the part, and the bone it is aimed at where the data gives it none
        local name = partName(part)
        local text = string.format('%s  %d%%%s', name, math.floor(shown + 0.5), crippled and '  crippled' or '')
        add(widgets.clickable(text, function()
            queueAttack(target, part, name)
        end, {
            enabled = left >= state.cost and part.chance > 0,
            color = partFocus and index == partPicked and colors.pickedColor or colors.normalColor,
        }))
    end

    if #queue > 0 then
        add(widgets.label('Queued', colors.hintColor, textSize - 4))
        for index, attack in ipairs(queue) do
            add(widgets.label(string.format('%d. %s %d%%', index, attack.name, math.floor(math.min(attack.chance, 100) + 0.5)),
                colors.normalColor, textSize - 2))
        end
    end
    add(widgets.row(
        widgets.cell(widgets.clickable('Attack', go, { enabled = #queue > 0, color = colors.titleColor }), 120),
        widgets.cell(widgets.clickable('Undo', function()
            queue[#queue] = nil
            renderWidgets()
        end, { enabled = #queue > 0, color = colors.titleColor }), 100),
        widgets.clickable('Cancel', function()
            queue = {}
            closeMode()
        end, { color = colors.titleColor })))
    element = ui.create(widgets.panel(rows))
end

local cycle, movePart, queuePicked, accept, giveBack

-- The part that the pick starts on: the first of the torso's group when the target has one, else the first part (the
-- executable also starts on the head when it is less than 300 units from the player; that rule was not traced to the end)
local function startPart()
    for index, part in ipairs(shownParts) do
        if part.group == TORSO_GROUP then return index end
    end
    return 1
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
        tellPlayer('No target in sight')
        return
    end
    picked, queue = 1, {}
    partPicked, partFocus = 1, false
    modeOpen = true
    I.UI.addMode('Interface', { windows = {} })
    vats.setOpen(true)
    fadeAge = 0
    cache, limbState = {}, {}
    if buildScreen() then
        shownParts = vats.parts(targets[picked])
        partPicked = startPart()
        partFocus = true
        screen:onClick('left_arrow', function() cycle(-1) end)
        screen:onClick('right_arrow', function() cycle(1) end)
        screen:onClick('accept_button', function() accept() end)
        screen:onClick('return_button', function() giveBack() end)
        screen:onBackgroundClick(function() queuePicked() end)
        screen:onRightClick(function() giveBack() end)
        -- the file counts the wheel the other way round (up is -1); the wheel up picks the next part, as W does
        screen:onWheel(function(notches) movePart(-notches) end)
        sound('UIVATSEnter')
        showQueue()
        updateScreen(0)
    else
        renderWidgets()
    end
end

-- A or D (and the pad's bumper): the previous or next target. With one target only there is nothing to pick (the file's
-- arrows play the sound of a refusal then)
function cycle(by)
    if not modeOpen then return end
    if #targets <= 1 then
        if screen then sound('UIVATSEnterFail') end
        return
    end
    picked = (picked - 1 + by) % #targets + 1
    partPicked = 1
    if screen then
        limbState = {}
        shownParts = vats.parts(targets[picked])
        partPicked = startPart()
    end
    render()
end

-- Moves the pick one part on in the target's list of parts (the next when `by` is positive, the previous when it is
-- negative), round the ends as the executable does; a part whose bracket is not on the screen is stepped over. The keys are
-- W for the next and S for the previous, the wheel up the next. The widgets step through the parts and stop at the ends.
function movePart(by)
    if not modeOpen then return end
    local target = targets[picked]
    if screen then
        local count = #shownParts
        if count == 0 then return end
        local step = by > 0 and 1 or -1
        local at = partPicked
        for _ = 1, count do
            at = (at - 1 + step) % count + 1
            local item = shownParts[at].item
            if item == nil or item.onScreen then
                partPicked = at
                break
            end
        end
        render()
        return
    end
    local parts = element and target and vats.parts(target) or {}
    if #parts == 0 then
        return
    end
    if partFocus then
        partPicked = math.max(1, math.min(#parts, partPicked + by))
    else
        partFocus, partPicked = true, by > 0 and 1 or #parts
    end
    renderWidgets()
end

-- Queues the attack on the part that is picked out, as a click on it does (not when the points or the chance do not allow);
-- on the widgets the first press only shows the part that is picked out
function queuePicked()
    if not modeOpen then return end
    local target = targets[picked]
    local parts = screen and shownParts or (element and target and vats.parts(target) or {})
    local part = parts[partPicked]
    if not part or not target then
        return
    end
    if not screen and not partFocus then
        -- nothing was picked out yet: the first press shows which part is
        partFocus = true
        renderWidgets()
        return
    end
    local state = vats.state()
    if part.chance <= 0 then
        -- the executable refuses a shot with no chance with a message and the sound of a refusal (0x7EEB50)
        sound('UIVATSEnterFail')
        tellPlayer(word('sVATSMessageZeroChance', 'The chance to hit is zero'))
    elseif state.actionPoints - spent() < state.cost then
        sound('UIVATSInsufficientAP')
    else
        queueAttack(target, part, partName(part))
    end
end

-- Accept: the screen is left, and the queue (when there is one) is carried out
function accept()
    if not modeOpen then return end
    if #queue == 0 then
        -- the button takes no click while the list is empty (the file's `target`); the key is the same, and the
        -- older widget screen has nothing to carry out either
        return
    end
    sound('UIVATSExit')
    sound('UIVATSReady')
    go()
end

-- Return: the last attack of the queue is taken off (its points come back); with nothing queued the screen is left
function giveBack()
    if not modeOpen then return end
    if #queue > 0 then
        queue[#queue] = nil
        sound('UIMenuCancel')
        render()
    else
        sound('UIVATSExit')
        closeMode()
    end
end

-- The next attack of the queue that is being carried out
local function step()
    local attack = table.remove(running, 1)
    -- The queue is over with its last attack, whatever comes of it: a target that is gone between two attacks (or an
    -- error) must not leave the queue set, because the screen does not open while it is. V.A.T.S. itself stays open
    -- for the engine through that last attack (its perks and effects see it), and ends after it.
    local last = #running == 0
    if last then
        running = nil
    end
    local attacked, result = false, nil
    if attack and attack.target:isValid() then
        attacked, result = pcall(vats.attack, attack.target, attack.part)
    end
    -- It also ends when the player can not go on
    if last or (attacked and (result.refused == 'points' or result.refused == 'ammo')) then
        endQueue()
    end
    if not attacked then
        if result then
            error(result, 0)
        end
        return
    end
    if result.refused then
        if result.refused == 'points' then
            tellPlayer('Not enough action points')
        elseif result.refused == 'ammo' then
            tellPlayer('Out of ammunition')
        end
    end
    -- (the game says nothing of a blow that landed or missed: the target, the experience of a kill and the meters show it)
end

local function isOpen()
    return modeOpen and (screen ~= nil or element ~= nil)
end

-- What a key (or a button of the pad that stands for one) does: V opens the screen; on the screen A and D (or the left and
-- right arrows) pick a character, W and S (or the up and down arrows) a part of it, Space queues the attack on the part, E
-- or Enter accepts and B takes the last attack off the queue
local function press(code)
    if messageOpen() then
        return
    end
    if code == input.KEY.V and not modeOpen and controlsAllowed() then
        open()
    elseif not isOpen() or I.UI.getMode() ~= 'Interface' then
        -- (a menu opened over the screen takes the keys and the buttons)
        return
    elseif code == input.KEY.Enter or code == input.KEY.E then
        accept()
    elseif code == input.KEY.Space then
        queuePicked()
    elseif code == input.KEY.B then
        giveBack()
    elseif code == input.KEY.RightArrow or code == input.KEY.D or code == input.KEY.Tab or code == input.KEY.PageDown then
        cycle(1)
    elseif code == input.KEY.LeftArrow or code == input.KEY.A or code == input.KEY.PageUp then
        cycle(-1)
    elseif code == input.KEY.DownArrow or code == input.KEY.S then
        -- on the screen S is the executable's previous part (and W the next); the widgets go down the list with S
        movePart(screen and -1 or 1)
    elseif code == input.KEY.UpArrow or code == input.KEY.W then
        movePart(screen and 1 or -1)
    end
end

-- On the pad the right trigger queues the attack on the part (the games' own: the file puts Select on it) and A accepts the
-- queue (the file's Accept, Enter here; the games' help says "Use: attack your target, A: accept your actions, B: cancel an
-- attack"); Y queues too and X carries the queue out, which were the port's first buttons. The left bumper opens the screen
-- and picks the part while it is open, the right bumper picks the character (A clicks where the pointer is while the
-- pad holds no pick, and where nothing takes the click it queues the attack as the attack control does)
local function padActive()
    return isOpen() and I.UI.getMode() == 'Interface' and not messageOpen()
end

local handlePad = pad.handler('vats', padActive, press,
    { [input.CONTROLLER_BUTTON.Y] = input.KEY.Space, [input.CONTROLLER_BUTTON.X] = input.KEY.Enter,
      [input.CONTROLLER_BUTTON.RightTrigger] = input.KEY.Space,
      [input.CONTROLLER_BUTTON.LeftShoulder] = input.KEY.W })

local function onPad(button)
    if messageOpen() then
        return
    end
    if not isOpen() and not modeOpen and button == input.CONTROLLER_BUTTON.LeftShoulder then
        if I.UI.getMode() == nil and not running and controlsAllowed() then
            pad.log('vats', button, 'open')
            open()
        end
        return
    end
    handlePad(button)
    -- The left bumper picks the part (the key W), which is no key that moves the pick by itself: it holds the pick like
    -- the arrows do, so A accepts after it (only while the screen has the pad: a menu over it leaves the pick alone)
    if button == input.CONTROLLER_BUTTON.LeftShoulder and padActive() then
        pad.holdPick()
    end
end

-- The screen that was left fades out (the file's seconds), and the one that is up is written from the frame's own time
local function onFrame()
    local dt = core.getRealFrameDuration()
    if fading then
        fading.left = fading.left - dt
        if fading.left <= 0 then
            fading.shown:close()
            fading = nil
        else
            fading.shown:set(fading.shown.menu:root(), 'alpha', math.floor(255 * fading.left / FADE_SECONDS + 0.5))
        end
    end
    if screen and modeOpen then
        -- A window that changes its size gives the menu its new size, and the blocks that sit by the edges of the screen
        -- (the file leaves them for the code to place) move with it
        local size = ui.screenSize()
        if size.x ~= tiles.size.x or size.y ~= tiles.size.y then
            screen:checkSize()
            tiles.size = size
            tiles.width = menuWidth(size)
            placeBlocks(screen.menu, tiles)
        end
        updateScreen(dt)
    end
end

return {
    engineHandlers = {
        onKeyPress = function(key)
            press(key.code)
        end,
        onControllerButtonPress = onPad,
        onFrame = onFrame,
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
        -- A button of the pad, sent by a script (the buttons of a real pad come as onControllerButtonPress)
        PadButton = function(data)
            onPad(data.button)
        end,
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
