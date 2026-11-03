-- The head-up display of Fallout 3 and New Vegas drawn from the games' own menu file (main/hud_main_menu.xml), as the
-- executables do it: the file holds the containers (health, action points, messages, the sneak meter, the experience
-- bar, the radiation, the enemy bar, the limbs, the crosshair) and the code of the game places them from the size of the
-- screen, makes their parts from the templates of HUDTemplates.xml, and writes the numbers into them. The places and
-- numbers here are what Mac rounds 12AL and 12AM read in FalloutNV.exe (Create 0x76BFE0 and the update 0x770430):
--   * the menu is 960 units high and as wide as the screen is (1280 on a screen that is taller than wide); the safe zone is
--     15 on each side, so the blocks keep 2 x 15 from the edges;
--   * a bar is a row of whole tick marks: width = round(_TotalWidth x share / _ImageWidth) x _ImageWidth, at least one
--     tick (the bar of the action points is anchored at its right edge, the one of the enemy in its middle);
--   * a message is shown alone, for the seconds it came with, with 0.35 s to fade in and out; a message that is the same as
--     the one queued last is dropped;
--   * the compass is a strip of 360 degrees of the interface atlas that is shown 345 texels at a time from the heading on.
-- What the exe fills in that the engine does not tell yet is left out (the weapon's name and condition, the damage
-- threshold, the place's name, subtitles, the text under the crosshair).
local ui = require('openfallout.ui')
local menuModule = require('scripts.omw.falloutmenu')

local module = {}

local FILE = 'main/hud_main_menu.xml'
local FADE_SECONDS = 0.35
-- How long the limbs and the face stay after the health changed, the enemy bar after its target changed (the default of
-- fEnemyHealthBarTimer in the executable)
local LIMB_SECONDS = 5
local ENEMY_SECONDS = 5.5
-- The strip of the compass: its texels all round, the window of it that is shown, and the degrees that stand for the width
-- of the strip
local STRIP_TEXELS = 1023
local WINDOW = 345
local FIELD = 120
local SAFE = 15

-- The limbs by the names that the engine gives them, and the tiles of the file they are
local LIMBS = {
    head = 'Head', torso = 'Torso', left_arm = 'Left_Arm', right_arm = 'Right_Arm', left_leg = 'Left_Leg',
    right_leg = 'Right_Leg',
}
local LIMB_ORDER = { 'head', 'torso', 'left_arm', 'right_arm', 'left_leg', 'right_leg' }

local Hud = {}
Hud.__index = Hud

-- The width of a bar of whole ticks
local function barWidth(total, tick, share, steps, least)
    share = math.max(0, math.min(1, share))
    local width = math.floor(total * share / tick / steps + 0.5) * steps * tick
    return math.max(width, tick * least)
end

local function share(now, most)
    if most == nil or most <= 0 then return 0 end
    return math.max(0, math.min(1, now / most))
end

-- Puts every tile of the display in its place and makes the parts that the code makes
local function build(self)
    local m = self.shown.menu
    local size = self.size
    local height = 960
    local width = size.y > size.x and 1280 or math.floor(size.x / size.y * 960)
    self.width = width
    local sx, sy = SAFE, SAFE
    local t = {}
    self.tiles = t

    local function find(name)
        local id = m:find(name)
        if not id then error('the file of the display has no tile ' .. name, 0) end
        return id
    end
    local function place(tile, x, y)
        if x then m:set(tile, 'x', x) end
        if y then m:set(tile, 'y', y) end
    end
    local function make(parent, template, name, traits)
        local tile = m:instantiate(parent, template, name)
        if not tile then error('the file of the display has no template ' .. template, 0) end
        for trait, value in pairs(traits or {}) do m:set(tile, trait, value) end
        return tile
    end

    m:set(m:root(), 'visible', 1)
    -- The tiles of the file that the code of the game shows only when it has something for them
    for _, name in ipairs({ 'HardcoreMode', 'CNDArrows', 'DDTIcon', 'DDTIconAP', 'DDTIconEnemy', 'DDTIconEnemyAP',
        'AmmoTypeLabel', 'BreathMeter', 'Explosive_positioning_rect', 'crippled_limb_indicator', 'SneakMeter',
        'Hokeys', 'QuestReminder', 'Region_Location', 'Subtitles', 'Info' }) do
        local id = m:find(name)
        if id then m:set(id, 'visible', 0) end
    end

    -- The containers
    local hitPoints, actionPoints = find('HitPoints'), find('ActionPoints')
    local hpY = height - 2 * sy - 127
    place(hitPoints, 2 * sx + 10, hpY)
    place(actionPoints, width - 2 * sx - 386 + 30, hpY)
    t.rads = find('RadiationMeter')
    place(t.rads, width - 2 * sx - 367 - 20, 2 * sy)
    t.enemy = find('EnemyHealth')
    place(t.enemy, math.floor(width / 2) - 175, hpY + 40)
    t.reticle = find('ReticleCenter')
    place(t.reticle, math.floor(width / 2) - 63 / 2 - 3, math.floor(height / 2) - 75 / 2 - 3)
    t.sneak = find('SneakMeter')
    place(t.sneak, math.floor(width / 2), 2 * sy)
    t.messages = find('Messages')
    place(t.messages, 2 * sx - 5, 2 * sy + 25)
    t.xp = find('XPMeter')
    place(t.xp, width - 2 * sx - 390, height - 2 * sy - 280)
    t.limbs = find('crippled_limb_indicator')
    place(t.limbs, width - 2 * sx - 200, 100 + 2 * sy)
    t.hitPoints, t.actionPoints = hitPoints, actionPoints

    -- Health: the bracket, the bar, the number and the compass with its three rows of marks
    make(hitPoints, 'template_left_bracket', 'hud_hp_bracket', { brightness = 255 })
    t.hpMeter = make(hitPoints, 'template_meter', 'hud_hp_meter', { _OriginalX = 20, x = 20, y = 41, brightness = 175 })
    t.hpText = make(hitPoints, 'template_justify_right_text', 'hud_hp_text', { x = 53, y = 8 })
    t.compass = make(hitPoints, 'template_compass_window', 'hud_compass_window', {})
    t.groups = {
        make(t.compass, 'template_compass_icon_group', 'hud_compass_group1', { y = 30, depth = 4 }),
        make(t.compass, 'template_compass_icon_group', 'hud_compass_group2', { y = 30, depth = 3 }),
        make(t.compass, 'template_compass_npc_icon_group', 'hud_compass_group3', { y = 10, depth = 2 }),
    }
    -- The marks of the compass: a few of each kind, taken in turn
    t.marks = { quest = {}, landmark = {}, tick = {} }
    for index = 1, 10 do
        t.marks.quest[index] = make(t.groups[1], 'template_compass_icon_quest', 'hud_mark_quest' .. index, { visible = 0 })
        t.marks.landmark[index] = make(t.groups[2], 'template_compass_icon_marker', 'hud_mark_landmark' .. index,
            { visible = 0 })
    end
    for index = 1, 8 do
        t.marks.tick[index] = make(t.groups[3], 'compass_npc_icon', 'hud_mark_tick' .. index, { visible = 0 })
    end
    -- The mark of the player is in the middle of the window
    t.player = make(t.groups[1], 'template_compass_icon_player', 'hud_mark_player', { _Heading = WINDOW / 2 - 20 })

    -- Action points: the bracket, the bar (it grows to the left), the number and the rounds
    make(actionPoints, 'template_right_bracket', 'hud_ap_bracket', { x = -37, brightness = 255 })
    t.apMeter = make(actionPoints, 'template_meter', 'hud_ap_meter', { _OriginalX = 326, y = 41 })
    t.apText = make(actionPoints, 'template_justify_right_text', 'hud_ap_text', { x = 315, y = 8 })
    t.ammoText = make(actionPoints, 'template_justify_right_text', 'hud_ammo_text', { x = 315, y = 80, visible = 0 })

    -- The enemy: the bracket, the bar (it grows from its middle) and the name
    make(t.enemy, 'template_enemy_health_bracket', 'hud_enemy_bracket', { brightness = 175, visible = 1 })
    t.enemyMeter = make(t.enemy, 'template_meter', 'hud_enemy_meter',
        { _OriginalX = 171, _TotalWidth = 240, y = 3, visible = 1, brightness = 175 })
    t.enemyName = make(t.enemy, 'template_justify_center_text', 'hud_enemy_name', { x = 170, y = 35, wrapwidth = 350 })
    m:set(t.enemy, 'systemcolor', 2)

    -- The crosshair
    t.crosshair = make(t.reticle, 'template_reticle_center', 'hud_reticle', {})

    -- The messages: the icon, the text and the bracket, all faded in and out together
    t.messageIcon = make(t.messages, 'template_message_icon', 'hud_message_icon', { x = 0, y = -25 })
    t.messageText = make(t.messages, 'template_justify_left_text', 'hud_message_text',
        { x = 130, y = 5, wrapwidth = 320, depth = 2, alpha = 0, visible = 1 })
    t.messageBracket = make(t.messages, 'template_message_bracket', 'hud_message_bracket', { brightness = 175 })

    -- The limbs, in the places and sizes that the code gives them
    t.limb = {}
    local head = find('Head')
    t.limb.head = head
    m:set(head, 'width', 63.75)
    m:set(head, 'height', 69)
    place(head, 0, 40)
    t.face = find('Face')
    m:set(t.face, 'width', 36)
    m:set(t.face, 'height', 48)
    place(t.face, (63.75 - 36) / 2, (69 - 48) / 2 + 40 + 12)
    local torso = find('Torso')
    t.limb.torso = torso
    m:set(torso, 'width', 76.5)
    m:set(torso, 'height', 96)
    place(torso, -10.5, 40 + 64.5)
    local torsoY = 40 + 64.5
    local leftArm = find('Left_Arm')
    t.limb.left_arm = leftArm
    m:set(leftArm, 'width', 75)
    m:set(leftArm, 'height', 39)
    place(leftArm, -10.5 + 67.5, torsoY)
    local rightArm = find('Right_Arm')
    t.limb.right_arm = rightArm
    m:set(rightArm, 'width', 72)
    m:set(rightArm, 'height', 40.5)
    place(rightArm, -10.5 - 66, torsoY - 3.75)
    local leftLeg = find('Left_Leg')
    t.limb.left_leg = leftLeg
    m:set(leftLeg, 'width', 54)
    m:set(leftLeg, 'height', 84)
    place(leftLeg, -10.5 + 35.25, torsoY + 64.5)
    local rightLeg = find('Right_Leg')
    t.limb.right_leg = rightLeg
    m:set(rightLeg, 'width', 63)
    m:set(rightLeg, 'height', 84)
    place(rightLeg, -10.5 - 27.75, torsoY + 62.25)

    -- The radiation, the experience: the tiles the file has, found by name
    t.radsBracket, t.radsPointer = find('radiation_bracket'), find('radiation_pointer')
    t.radsValue, t.radsText = find('radiation_text_value'), find('radiation_text')
    t.xpBracket, t.xpPointer, t.xpAmount = find('XPBracket'), find('XPPointer'), find('XPAmount')
    t.xpLabel, t.xpLast, t.xpNext = find('XPLabel'), find('XPLastLevel'), find('XPNextLevel')
    t.sneakText = find('sneak_nif')
    t.xpMin = m:getNumber(t.xpPointer, '_x_min')
    t.xpMax = m:getNumber(t.xpPointer, '_x_max')
    t.tick = { hp = m:getNumber(t.hpMeter, '_ImageWidth'), ap = m:getNumber(t.apMeter, '_ImageWidth'),
        enemy = m:getNumber(t.enemyMeter, '_ImageWidth') }
    t.total = { hp = m:getNumber(t.hpMeter, '_TotalWidth'), ap = m:getNumber(t.apMeter, '_TotalWidth'),
        enemy = m:getNumber(t.enemyMeter, '_TotalWidth') }
    t.reticlePlain = m:getText(t.crosshair, 'user0')
    t.reticleFilled = m:getText(t.crosshair, 'user1')
end

-- A trait is set only when it is not what it was set to last, so a frame that changes nothing draws nothing again
function Hud:put(tile, trait, value)
    local key = tile .. '|' .. trait
    if self.cache[key] == value then return end
    self.cache[key] = value
    self.shown.menu:set(tile, trait, value)
end

-- A message is taken in when the one queued last is not the same, and waits for its turn
function Hud:queueMessages(messages)
    for _, message in ipairs(messages) do
        local last = self.queue[#self.queue]
        if not (last and last.text == message.text) then
            self.queue[#self.queue + 1] = { text = message.text, icon = message.icon, seconds = message.seconds or 2 }
        end
    end
end

-- The message that is shown and how far it has faded in (or out)
function Hud:stepMessages(dt)
    local head = self.queue[1]
    if head then
        self.leaving = nil
        head.age = (head.age or 0) + dt
        if head.age >= head.seconds then
            table.remove(self.queue, 1)
            local following = self.queue[1]
            if following then
                following.age = 0
            else
                self.leaving = { message = head, left = FADE_SECONDS }
            end
            head = following
        end
    elseif self.leaving then
        self.leaving.left = self.leaving.left - dt
        if self.leaving.left <= 0 then self.leaving = nil end
    end
    if head then return head, math.min(1, head.age / FADE_SECONDS) end
    if self.leaving then return self.leaving.message, math.max(0, self.leaving.left / FADE_SECONDS) end
    return nil, 0
end

local function iconPath(icon)
    if type(icon) ~= 'string' or icon == '' then return nil end
    return icon
end

function Hud:showMessage(dt)
    local t = self.tiles
    local message, fade = self:stepMessages(dt)
    local alpha = message and math.floor(255 * fade + 0.5) or 0
    local icon = message and iconPath(message.icon)
    self:put(t.messageText, 'string', message and message.text or '')
    self:put(t.messageText, 'alpha', alpha)
    self:put(t.messageBracket, 'alpha', alpha)
    if icon then
        -- A picture that is not in the atlas is the file itself
        self:put(t.messageIcon, 'filename', icon)
    end
    self:put(t.messageIcon, 'alpha', icon and alpha or 0)
end

-- The face and the limbs come up when the health changed, for a few seconds
function Hud:showLimbs(state, dt)
    local t = self.tiles
    local crippled = {}
    for _, limb in ipairs(state.crippled or {}) do crippled[limb] = true end
    local key = state.health .. '/' .. state.maxHealth .. '|' .. table.concat(state.crippled or {}, ',')
    if self.limbKey ~= nil and key ~= self.limbKey then self.limbLeft = LIMB_SECONDS end
    self.limbKey = key
    local visible = (self.limbLeft or 0) > 0 or next(crippled) ~= nil
    if self.limbLeft and self.limbLeft > 0 then self.limbLeft = self.limbLeft - dt end
    self:put(t.limbs, 'visible', visible and 1 or 0)
    if not visible then return end
    local lost = state.maxHealth > 0 and (state.maxHealth - state.health) / state.maxHealth or 0
    local face = math.max(0, math.min(5, math.floor(5 * lost + 0.5)))
    self:put(t.face, 'filename', string.format('Interface\\Stats\\face_%02d.dds', face))
    self:put(t.face, 'alpha', 255)
    for _, limb in ipairs(LIMB_ORDER) do
        local tile = t.limb[limb]
        self:put(tile, 'filename',
            'Interface\\Stats\\' .. limb .. (crippled[limb] and '_broken' or '') .. '.dds')
        self:put(tile, 'systemcolor', crippled[limb] and 2 or 1)
        self:put(tile, 'alpha', 255)
    end
end

function Hud:showExperience(state)
    local t = self.tiles
    local on = state.experienceShare ~= nil
    self:put(t.xp, 'visible', on and 1 or 0)
    if not on then return end
    self:put(t.xpBracket, 'visible', 1)
    self:put(t.xpBracket, 'alpha', 255)
    self:put(t.xpPointer, 'visible', 1)
    self:put(t.xpPointer, 'x', t.xpMin + state.experienceShare * (t.xpMax - t.xpMin))
    self:put(t.xpAmount, 'visible', state.gain ~= 0 and 1 or 0)
    self:put(t.xpAmount, 'string', (state.gain > 0 and '+' or '') .. state.gain)
    self:put(t.xpLabel, 'visible', 1)
    self:put(t.xpLast, 'visible', 1)
    self:put(t.xpLast, 'string', tostring(state.levelStart or ''))
    self:put(t.xpNext, 'visible', 1)
    self:put(t.xpNext, 'string', tostring(state.nextLevel or ''))
end

function Hud:showRadiation(state)
    local t = self.tiles
    local on = state.rads > 0
    local alpha = on and 255 or 0
    self:put(t.rads, 'visible', 1)
    self:put(t.radsBracket, 'alpha', alpha)
    self:put(t.radsPointer, 'alpha', alpha)
    self:put(t.radsValue, 'alpha', alpha)
    self:put(t.radsValue, 'string', on and ('+' .. state.rads) or '')
    self:put(t.radsText, 'alpha', 0)
    -- The pointer runs along the bracket as the radiation grows
    self:put(t.radsPointer, 'x', 15 + 300 * share(state.rads, state.radsMax))
end

-- The bar of an enemy stays for a while after its target or its health changed, then goes
function Hud:showEnemy(state, dt)
    local t = self.tiles
    local target = state.target
    if target == nil then
        self.enemyKey = nil
        self:put(t.enemy, 'visible', 0)
        return
    end
    local ratio = share(target.health, target.maxHealth)
    local key = target.name .. '|' .. ratio
    if key ~= self.enemyKey then
        self.enemyKey = key
        self.enemyLeft = ENEMY_SECONDS
    end
    self.enemyLeft = (self.enemyLeft or 0) - dt
    local on = self.enemyLeft > 0
    self:put(t.enemy, 'visible', on and 1 or 0)
    if not on then return end
    local width = barWidth(t.total.enemy, t.tick.enemy, ratio, 2, ratio > 0 and 1 or 0)
    self:put(t.enemyMeter, 'width', width)
    self:put(t.enemyMeter, 'x', 171 - width / 2)
    self:put(t.enemyName, 'string', target.name)
end

function Hud:showCompass(compass)
    local t = self.tiles
    if compass == nil then return end
    local scroll = compass.heading / 360 * STRIP_TEXELS - WINDOW / 2
    self:put(t.compass, '_sourcex', scroll)
    local used = { quest = 0, landmark = 0, tick = 0 }
    for _, marker in ipairs(compass.markers or {}) do
        local kind = marker.kind == 'quest' and 'quest' or marker.kind == 'landmark' and 'landmark' or 'tick'
        local pool = t.marks[kind]
        used[kind] = used[kind] + 1
        local tile = pool[used[kind]]
        if tile then
            -- The strip is 1023 texels for 360 degrees, the engine's share is of the 120 degrees in front
            local x = WINDOW / 2 + (marker.share - 0.5) * FIELD / 360 * STRIP_TEXELS
            local half = kind == 'tick' and 16 or 20
            self:put(tile, '_Heading', math.floor(x - half + 0.5))
            self:put(tile, 'visible', 1)
        end
    end
    for kind, pool in pairs(t.marks) do
        for index = used[kind] + 1, #pool do self:put(pool[index], 'visible', 0) end
    end
end

-- Shows what the player is now. `state` is what the status script gathers, `mode` is 'game' for the whole display and
-- 'messages' while a window is open (the display then shows only its messages)
function Hud:update(state, dt)
    local size = ui.screenSize()
    if size.x ~= self.size.x or size.y ~= self.size.y then return false end
    local t = self.tiles
    local full = state.mode == 'game'
    local off = state.mode == 'off'
    self:queueMessages(state.messages or {})
    self.shown:batch(function()
        self:showMessage(dt)
        self:put(t.messages, 'visible', off and 0 or 1)
        for _, tile in ipairs({ t.hitPoints, t.actionPoints, t.rads, t.reticle, t.sneak, t.xp, t.limbs, t.enemy }) do
            if not full then self:put(tile, 'visible', 0) end
        end
        if not full then return end
        -- While a script holds the player still the display is the executable's mode 0xC: the radiation, the experience and
        -- the messages (Mac round 12AM B5; the other tiles of that mode are not made yet)
        local scripted = state.scripted == true
        self:put(t.hitPoints, 'visible', scripted and 0 or 1)
        self:put(t.actionPoints, 'visible', scripted and 0 or 1)
        if scripted then
            for _, tile in ipairs({ t.reticle, t.sneak, t.limbs, t.enemy }) do self:put(tile, 'visible', 0) end
            self:showExperience(state)
            self:showRadiation(state)
            return
        end
        -- Health
        self:put(t.hpMeter, 'width', barWidth(t.total.hp, t.tick.hp, share(state.health, state.maxHealth), 1,
            state.health > 0 and 1 or 0))
        self:put(t.hpText, 'string', tostring(state.health))
        -- Action points and the rounds of the weapon
        local ap = barWidth(t.total.ap, t.tick.ap, share(state.points, state.maxPoints), 1, state.points > 0 and 1 or 0)
        self:put(t.apMeter, 'width', ap)
        self:put(t.apMeter, 'x', 326 - ap)
        self:put(t.apText, 'string', tostring(state.points))
        self:put(t.ammoText, 'visible', state.rounds ~= nil and 1 or 0)
        self:put(t.ammoText, 'string', state.rounds ~= nil and tostring(state.rounds) or '')
        -- The crosshair
        self:put(t.reticle, 'visible', state.reticle and 1 or 0)
        local filled = state.reticle and state.targetHostile
        self:put(t.crosshair, 'filename', filled and t.reticleFilled or t.reticlePlain)
        self:put(t.crosshair, 'systemcolor', filled and 2 or 1)
        -- Sneaking
        self:put(t.sneak, 'visible', state.sneaking and 1 or 0)
        if state.sneaking then
            self:put(t.sneakText, 'string', state.sneakText)
            local alert = state.sneak == 'caution' or state.sneak == 'danger'
            self:put(t.sneakText, 'systemcolor', alert and 2 or 1)
        end
        self:showExperience(state)
        self:showRadiation(state)
        self:showEnemy(state, dt)
        self:showLimbs(state, dt)
    end)
    return true
end

-- The marks of the compass move with each turn, so they are set apart from the rest
function Hud:updateCompass(compass, full)
    if not full then return end
    self.shown:batch(function() self:showCompass(compass) end)
end

function Hud:close()
    self.shown:close()
end

-- The display, or nil when the file of the game cannot be shown
function module.create()
    local size = ui.screenSize()
    local shown = menuModule.open(FILE, { layer = 'HUD', passive = true })
    if not shown then return nil end
    local self = setmetatable({ shown = shown, size = size, cache = {}, queue = {} }, Hud)
    local ok, problem = pcall(build, self)
    if not ok then
        print('The head-up display is not drawn from its file: ' .. tostring(problem))
        shown:close()
        return nil
    end
    shown:refresh()
    return self
end

return module
