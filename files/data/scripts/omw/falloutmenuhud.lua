-- The head-up display of Fallout 3 and New Vegas drawn from the games' own menu file (main/hud_main_menu.xml), as the
-- executables do it: the file holds the containers (health, action points, messages, the sneak meter, the experience
-- bar, the radiation, the enemy bar, the limbs, the crosshair) and the code of the game places them from the size of the
-- screen, makes their parts from the templates of HUDTemplates.xml, and writes the numbers into them. The places and
-- numbers here are what Mac rounds 12AL and 12AM read in FalloutNV.exe (Create 0x76BFE0 and the update 0x770430):
--   * the menu is 960 units high and as wide as the screen is (1280 on a screen that is taller than wide); the safe zone is
--     15 on each side, so the blocks keep 2 x 15 from the edges;
--   * a bar is a row of whole tick marks: width = round(_TotalWidth x share / _ImageWidth) x _ImageWidth, at least one
--     tick (the bar of the action points is anchored at its right edge, the one of the enemy in its middle);
--   * a message is shown alone, for the seconds it came with, with 0.35 s to fade in and out; the seconds of the first
--     message after a pause count from the end of its fade in, and the next one follows at once at full strength (the
--     fade out comes only when the queue is empty; Mac round 12AW S11); a message that is the same as the one queued
--     last is dropped;
--   * the compass is a strip of 360 degrees of the interface atlas that is shown 345 texels at a time, its texture shifted by
--     the heading over 360 less 0.15 (Mac round 12AN B8); a marker is drawn when 1.45 times its bearing is within 70 degrees
--     of the front, at x = (1.45 x bearing + 70) x 274 / 140, and fades near the two ends.
--   * the text under the crosshair (Info) is three lines in the middle of the bottom: the word of what the player does with
--     the thing (with the key that does it, or the button of the pad) over its name, and a row of its weight and value
--     (or, for a locked thing, what the lock is, in brackets) under it (Mac rounds 12AM B2 and 12AN B7).
--   * the line of text over the compass is written by two events (Mac rounds 12AO B3, 12AQ B1 and 12AV S1): the name of the
--     cell the player comes to after a door, a teleport or a load is written and waits for the tile to be on the screen, then
--     fades in over 2 s; the name of the region the player comes into is shown only when a line that is on the screen has
--     another text (and then fades in from nothing, the old text gone), else it is only written. The fade out begins 4 s
--     after the fade in did and takes 2 s more. A second line for the name of a place is in the file, but the executable never
--     shows it.
--   * the quest text is the block QuestReminder, 170 under the top edge of the safe zone: an event (a quest was added, completed
--     or failed, a place was found) writes its heading over its name, which comes up one character at a time, and the lines
--     of the objectives come one after the other under it (falloutmenuquests.lua keeps the times: Mac round 12AR C1).
-- What the exe fills in that the engine does not tell yet is left out (the weapon's name and condition, the damage
-- threshold).
local ui = require('openfallout.ui')
local vfs = require('openfallout.vfs')
local menuModule = require('scripts.omw.falloutmenu')
local hotKeys = require('scripts.omw.falloutmenuhotkeys')
local subtitles = require('scripts.omw.falloutmenusubtitles')
local questText = require('scripts.omw.falloutmenuquests')

local module = {}

local FILE = 'main/hud_main_menu.xml'
local FADE_SECONDS = 0.35
-- How long the limbs and the face stay after the health changed, the enemy bar after its target changed (the default of
-- fEnemyHealthBarTimer in the executable)
local LIMB_SECONDS = 5
local ENEMY_SECONDS = 5.5
-- The strip of the compass: its texels all round, the window of it that is shown, where the heading is in the texture (0.15
-- of the way round) and how far across the strip the marks go (274 texels, from x 0)
local STRIP_TEXELS = 1023
local STRIP_HEADING = 0.15
local MARK_SPAN = 274
-- Where a mark stands that is straight ahead, and how far from the ends the marks begin to fade
local MARK_FRONT = 137
local MARK_FADE = 55
local SAFE = 15
-- The line of the region name: seconds it takes to fade in and out, and from the start of the fade in until the fade out may
-- begin, and the alpha it fades in to (the executable takes 255 times its setting fHudOpacity:Interface, 1.0 by default)
local REGION_FADE = 2
local REGION_HOLD = 4
local REGION_ALPHA = 255

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
    self.make = make

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
    -- The text under the crosshair: the block is 320 wide, centred, 110 above the lower safe edge; the verb is the hot rect
    -- of the file (it keeps the word and the key or the button next to it), the code makes the other lines
    t.info = find('Info')
    place(t.info, math.floor(width / 2) - 160, height - 2 * sy - 80 - 30)
    t.infoVerb = find('justify_center_hotrect')
    m:set(t.infoVerb, '_x', 160)
    t.infoName = make(t.info, 'template_justify_center_text', 'hud_info_name', { x = 160, y = 0, wrapwidth = 320 })
    t.infoStatus = make(t.info, 'template_justify_center_text', 'hud_info_status', { x = 160, y = 65, alpha = 0 })
    t.infoWeightLabel = make(t.info, 'template_justify_left_text', 'hud_info_weight_label', { x = 10, y = 60 })
    t.infoWeight = make(t.info, 'template_justify_right_text', 'hud_info_weight', { x = 135, y = 60 })
    t.infoValueLabel = make(t.info, 'template_justify_left_text', 'hud_info_value_label', { x = 165, y = 60 })
    t.infoValue = make(t.info, 'template_justify_right_text', 'hud_info_value', { x = 290, y = 60 })
    t.xp = find('XPMeter')
    place(t.xp, width - 2 * sx - 390, height - 2 * sy - 280)
    -- The hot keys: the block is 300 by 300, over the action points and 20 above the health's row. The code makes the
    -- selector (the big picture of the item and, when a key is held, the wheel of the eight slots) and the line with the
    -- item's name, which stay hidden until a key is pressed
    t.hotkeys = find('Hokeys')
    place(t.hotkeys, width - 2 * sx - 386 + 30 + 15, height - 2 * sy - 127 - 300 - 20)
    t.hotSelector = make(t.hotkeys, 'template_hotkey_selector', 'hud_hotkey_selector', { visible = 0 })
    t.hotText = make(t.hotkeys, 'template_justify_center_text', 'hud_hotkey_text', { x = 150, y = -50, visible = 0 })
    t.hotSlots = {}
    for slot = 0, 7 do
        local tile = m:find('HK_Item_' .. slot)
        if tile then
            t.hotSlots[slot] = tile
            m:set(tile, 'visible', 0)
        end
    end
    t.hotAmmo = m:find('IM_AmmoSwap')
    if t.hotAmmo then m:set(t.hotAmmo, 'visible', 0) end
    -- The subtitles: the tile is 800 by 110 and empty in the file; the code makes a centred line of text in it, 50 above the
    -- block of the text under the crosshair (Mac round 12AQ B3)
    t.subtitles = find('Subtitles')
    place(t.subtitles, math.floor(width / 2) - 400, height - 2 * sy - 80 - 30 - 110 - 50)
    t.subtitleText = make(t.subtitles, 'template_justify_center_text', 'hud_subtitle_text', { x = 400, y = 0, wrapwidth = 800 })
    -- The region name: the tile is 100 by 100, 5 to the right of the health block and 75 above its top; the code makes a
    -- left-justified line of text in it, one line of the font down (Mac round 12AQ B1)
    t.region = find('Region_Location')
    place(t.region, 2 * sx + 15, hpY - 75)
    local measured = m:measure(7, 'Ag', 2048)
    local lineHeight = measured and measured.height or 0
    t.regionText = make(t.region, 'template_justify_left_text', 'hud_region_text',
        { x = 0, y = lineHeight > 0 and lineHeight or 24, wrapwidth = 2048, alpha = 0, visible = 1 })
    -- The quest text: the block is 400 by 200, 170 under the top safe edge and 15 in from the left one; the lines of
    -- the objectives are in the tile QuestStages (40 down) and the name of an event in QuestAdded (80 down), where the
    -- code makes the tiles it needs (a heading, a tile for each character of the name, a line and a box for each objective)
    t.quest = find('QuestReminder')
    place(t.quest, 2 * sx + 15, 2 * sy + 170)
    t.questStages, t.questAdded = find('QuestStages'), find('QuestAdded')
    t.questHeading = make(t.questAdded, 'template_justify_left_text', 'hud_quest_heading', {})
    t.questChars, t.questLines, t.questBoxes = {}, {}, {}
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
    -- The mark of the player is straight ahead (the x of it was not read: it is where a mark ahead stands)
    t.player = make(t.groups[1], 'template_compass_icon_player', 'hud_mark_player', { _Heading = MARK_FRONT })

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

-- A message is taken in when the one queued last is not the same, and waits for its turn. One that asks to go before the
-- others (the executable's front flag, Mac round 12AV S4) ends the one that is on at once and comes next.
function Hud:queueMessages(messages)
    for _, message in ipairs(messages) do
        local last = self.queue[#self.queue]
        if not (last and last.text == message.text) then
            local entry = {
                text = message.text,
                icon = message.icon,
                kind = message.kind or 0,
                sound = message.sound,
                seconds = message.seconds or 2,
            }
            if message.front and last then
                self.queue[1].seconds = 0
                table.insert(self.queue, 2, entry)
            else
                self.queue[#self.queue + 1] = entry
            end
        end
    end
end

-- The message that is shown and how far it has faded in (or out). The first message after a pause fades in for FADE_SECONDS
-- and its seconds are counted from the end of that; one that follows another is on the screen at once, and the fade out
-- comes only after the last (the executable stamps the time when the fade in has finished, and at the turn of a message
-- that follows: 0x7714F9, 0x7756E0).
function Hud:stepMessages(dt)
    local head = self.queue[1]
    if head then
        self.leaving = nil
        head.age = (head.age or 0) + dt
        if head.age >= head.seconds + (head.followed and 0 or FADE_SECONDS) then
            table.remove(self.queue, 1)
            local following = self.queue[1]
            if following then
                following.age = 0
                following.followed = true
            else
                self.leaving = { message = head, left = FADE_SECONDS }
            end
            head = following
        end
    elseif self.leaving then
        self.leaving.left = self.leaving.left - dt
        if self.leaving.left <= 0 then self.leaving = nil end
    end
    if head then return head, head.followed and 1 or math.min(1, head.age / FADE_SECONDS) end
    if self.leaving then return self.leaving.message, math.max(0, self.leaving.left / FADE_SECONDS) end
    return nil, 0
end

-- The face of the Vault Boy that a message without a picture gets (the executable's jump table 0x775658; a path wins).
-- The executable's notes name the first one in full (glow_message_vaultboy_neutral) and the others by their kind; the file
-- that exists is the one used.
local KIND_NAMES = { [0] = 'neutral', 'very_happy', 'sad', 'in_pain' }
local MESSAGE_ICONS = 'Interface/Icons/Message Icons/'
local kindIcons = {}

local function kindIcon(kind)
    kind = KIND_NAMES[kind or 0] and kind or 0
    local found = kindIcons[kind]
    if found == nil then
        found = false
        local name = KIND_NAMES[kind]
        for _, candidate in ipairs({ 'glow_message_vaultboy_' .. name, 'glow_message_' .. name, name }) do
            local path = MESSAGE_ICONS .. candidate .. '.dds'
            -- (the file is under textures/; the menus add that folder themselves, so the path kept is without it)
            local has, present = pcall(vfs.fileExists, 'textures/' .. path)
            if has and present then
                found = path
                break
            end
        end
        kindIcons[kind] = found
    end
    return found or nil
end

local function iconPath(icon, kind)
    if type(icon) ~= 'string' or icon == '' then return kindIcon(kind) end
    return icon
end

function Hud:showMessage(dt)
    local t = self.tiles
    local message, fade = self:stepMessages(dt)
    local alpha = message and math.floor(255 * fade + 0.5) or 0
    local icon = message and iconPath(message.icon, message.kind)
    -- The sound of a message plays when it comes on
    if message and message.sound and message.sound ~= '' and not message.played then
        message.played = true
        self.shown:playSound(message.sound)
    end
    self:put(t.messageText, 'string', message and message.text or '')
    self:put(t.messageText, 'alpha', alpha)
    self:put(t.messageBracket, 'alpha', alpha)
    if icon then
        -- A picture that is not in the atlas is the file itself
        self:put(t.messageIcon, 'filename', icon)
    end
    self:put(t.messageIcon, 'alpha', icon and alpha or 0)
end

-- A fade of the text of the region line from one alpha to another over some seconds (the executable's 0xA07C60: it starts at
-- the first alpha at once, and a fade that runs is cancelled)
local function startRegionFade(r, from, to, seconds)
    if from == to or seconds <= 0 then return end
    r.alpha = from
    r.fade = { from = from, to = to, seconds = seconds, age = 0 }
end

-- The line with the name of the region or the cell the player came into (FalloutNV.exe 0x7728F0 for a region, 0x772C30 for a
-- cell and the update at 0x770430). `events` are what the engine took since the last time: { name = the text, cell = true when
-- it is the name of a cell after a transition, false for a region }. Nothing but the text, the alpha and, for the tile, whether
-- the modes of the game show it are written, as in the executable
function Hud:showRegion(events, full, dt)
    local r = self.region
    r.clock = r.clock + dt
    for _, event in ipairs(events) do
        local name = event.name
        if name ~= '' then
            if event.cell then
                -- The name of the cell waits (_Hold) for the tile to be on the screen and for the fade to start from the update
                r.text, r.hold = name, true
            else
                -- A region: a line that is not on the screen only takes the text (it is shown when a name of a cell comes, or
                -- the next name of a region that is another than this one while the line shows); a line that is shows the
                -- name from nothing unless it says the same
                local same = r.alpha <= 0 or r.text == name
                r.hold, r.text = false, name
                if not same then
                    startRegionFade(r, 0, REGION_ALPHA, REGION_FADE)
                    r.stamp = r.clock
                end
            end
        end
    end
    local fade = r.fade
    if fade then
        fade.age = fade.age + dt
        if fade.age >= fade.seconds then
            r.alpha, r.fade = fade.to, nil
        else
            r.alpha = fade.from + (fade.to - fade.from) * fade.age / fade.seconds
        end
    end
    -- The fade out begins when the line is on the screen, no fade runs and more than REGION_HOLD s have passed since the start
    if r.alpha > 0 and r.fade == nil and r.clock - r.stamp > REGION_HOLD then
        startRegionFade(r, r.alpha, 0, REGION_FADE)
    end
    -- The name of a cell begins to fade in when the tile is shown by the mode of the game
    if r.hold and full then
        startRegionFade(r, 0, REGION_ALPHA, REGION_FADE)
        r.stamp, r.hold = r.clock, false
    end
    local t = self.tiles
    self:put(t.region, 'visible', full and 1 or 0)
    self:put(t.regionText, 'string', r.text)
    self:put(t.regionText, 'alpha', math.floor(r.alpha + 0.5))
end

-- A tile of the quest text from a pool: made from its template when it is first wanted
function Hud:questTile(pool, index, parent, template, name, traits)
    local list = self.tiles[pool]
    local tile = list[index]
    if not tile then
        tile = self.make(parent, template, name .. index, traits)
        list[index] = tile
    end
    return tile
end

local BOX_FILLED = 'Interface\\Shared\\Marker\\glow_square_filled_small.dds'
local BOX_EMPTY = 'Interface\\Shared\\Marker\\glow_square_small.dds'

-- The quest text (falloutmenuquests.lua says what is on the screen): the heading and the characters of the name of an
-- event, and the lines of the objectives with a box before each
function Hud:showQuests(full, dt)
    local t = self.tiles
    local menu = self.shown.menu
    self.quests:step(dt, {
        active = full,
        width = self.width,
        measure = function(font, text, wrap) return menu:measure(font, text, wrap) end,
    })
    for _, sound in ipairs(self.quests:takeSounds()) do self.shown:playSound(sound) end
    self:put(t.quest, 'visible', full and 1 or 0)
    local view = self.quests:view()
    local heading = view.heading
    self:put(t.questHeading, 'visible', heading and heading.visible and 1 or 0)
    if heading then
        self:put(t.questHeading, 'string', heading.text)
        self:put(t.questHeading, 'y', heading.y)
        self:put(t.questHeading, 'alpha', heading.alpha)
    end
    for index, char in ipairs(view.chars) do
        local tile = self:questTile('questChars', index, t.questAdded, 'template_justify_left_text', 'hud_quest_char',
            { font = char.font })
        self:put(tile, 'string', char.text)
        self:put(tile, 'x', char.x)
        self:put(tile, 'y', char.y)
        self:put(tile, 'alpha', char.alpha)
        self:put(tile, 'visible', char.visible and 1 or 0)
    end
    for index = #view.chars + 1, #t.questChars do self:put(t.questChars[index], 'visible', 0) end
    for index, line in ipairs(view.lines) do
        local tile = self:questTile('questLines', index, t.questStages, 'template_justify_left_text', 'hud_quest_line', {})
        local box = self:questTile('questBoxes', index, t.questStages, 'template_filled_checkbox', 'hud_quest_box', {})
        local on = line.visible and 1 or 0
        self:put(tile, 'string', line.text)
        self:put(tile, 'wrapwidth', line.wrap)
        self:put(tile, 'y', line.y)
        self:put(tile, 'alpha', line.alpha)
        self:put(tile, 'visible', on)
        self:put(box, 'filename', line.completed and BOX_FILLED or BOX_EMPTY)
        self:put(box, 'y', line.y + 3)
        self:put(box, 'alpha', line.alpha)
        self:put(box, 'visible', on)
    end
    for index = #view.lines + 1, #t.questLines do
        self:put(t.questLines[index], 'visible', 0)
        self:put(t.questBoxes[index], 'visible', 0)
    end
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

-- The text under the crosshair: what the player can do with the thing, what it is called, what it weighs and is worth, or
-- how its lock stands
function Hud:showInfo(info)
    local t = self.tiles
    if info == nil then
        self:put(t.info, 'visible', 0)
        return
    end
    self:put(t.info, 'visible', 1)
    local verb = info.verb ~= nil and info.verb ~= ''
    self:put(t.infoVerb, 'visible', verb and 1 or 0)
    if verb then
        self:put(t.infoVerb, 'string', info.verb)
        -- The key is written on the PC and the button of the pad (the button's picture is the exe's own token) when a pad is used
        self:put(t.infoVerb, '_PCButtonText', info.keyText or '')
    end
    self:put(t.infoName, 'string', info.name)
    local status = info.status ~= nil and info.status ~= ''
    self:put(t.infoStatus, 'string', status and info.status or '')
    self:put(t.infoStatus, 'alpha', status and 255 or 0)
    local stats = info.stats
    for _, tile in ipairs({ t.infoWeightLabel, t.infoValueLabel }) do
        self:put(tile, 'visible', stats and 1 or 0)
        self:put(tile, 'alpha', stats and 255 or 0)
    end
    self:put(t.infoWeight, 'visible', stats and 1 or 0)
    self:put(t.infoValue, 'visible', stats and 1 or 0)
    if stats then
        self:put(t.infoWeightLabel, 'string', stats.weightLabel)
        self:put(t.infoWeight, 'string', stats.weight)
        self:put(t.infoValueLabel, 'string', stats.valueLabel)
        self:put(t.infoValue, 'string', stats.value)
    end
end

-- The hot keys: `view` is what falloutmenuhotkeys.lua says (nothing when no key was pressed lately). The picture and the
-- name of the item come up when a key is pressed; the wheel, with the eight slots, while one is held.
function Hud:showHotKeys(view)
    local t = self.tiles
    local on = view ~= nil
    self:put(t.hotkeys, 'visible', on and 1 or 0)
    self:put(t.hotSelector, 'visible', on and 1 or 0)
    if not on then
        self:put(t.hotText, 'visible', 0)
        return
    end
    local preview = view.selector
    self:put(t.hotSelector, '_IconFilename', preview and preview.icon or '')
    self:put(t.hotSelector, '_IconVisible', preview and preview.icon and 1 or 0)
    self:put(t.hotText, 'visible', preview and 1 or 0)
    self:put(t.hotText, 'string', preview and preview.text or '')
    local wheel = view.wheel == true
    self:put(t.hotSelector, '_SelectedHotkey', wheel and view.selected or -1)
    if t.hotAmmo then self:put(t.hotAmmo, 'visible', wheel and 1 or 0) end
    for slot, tile in pairs(t.hotSlots) do
        self:put(tile, 'visible', wheel and 1 or 0)
        local row = wheel and view.slots and view.slots[slot + 1] or nil
        local assigned = row ~= nil and row.id ~= nil and row.id ~= ''
        local icon = assigned and hotKeys.iconPath(row.icon) or nil
        self:put(tile, '_HotKeyIcon', icon or '')
        self:put(tile, '_HotKeyAssigned', icon and 1 or 0)
    end
    if wheel then
        -- The name of the item of the lit slot is under the picture
        local row = view.slots and view.slots[(view.selected or 0) + 1] or nil
        local text = hotKeys.itemText(row)
        self:put(t.hotText, 'visible', text ~= '' and 1 or 0)
        self:put(t.hotText, 'string', text)
    end
end

function Hud:updateHotKeys(view)
    self.shown:batch(function() self:showHotKeys(view) end)
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
    -- The texture of the strip is shifted by the heading's share of the way round, less 0.15, and wraps
    local scroll = ((compass.heading / 360 - STRIP_HEADING) % 1) * STRIP_TEXELS
    self:put(t.compass, '_sourcex', scroll)
    local used = { quest = 0, landmark = 0, tick = 0 }
    for _, marker in ipairs(compass.markers or {}) do
        local kind = marker.kind == 'quest' and 'quest' or marker.kind == 'landmark' and 'landmark' or 'tick'
        local pool = t.marks[kind]
        used[kind] = used[kind] + 1
        local tile = pool[used[kind]]
        if tile then
            -- The share is across the span the marks have (the engine gives it for the bearings that are shown)
            local x = marker.share * MARK_SPAN
            -- The marks are fully lit up to 1.6 x 137 and fade to nothing at the end (the edge on the left is the same,
            -- which the executable's code was not read for)
            local fade = math.min(1, x / MARK_FADE, (MARK_SPAN - x) / MARK_FADE)
            self:put(tile, '_Heading', math.floor(x + 0.5))
            self:put(tile, 'alpha', math.floor(255 * math.max(0, fade) + 0.5))
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
    -- The file is opened for the keyboard or for the pad (what its key or button hint shows): when the player changes the
    -- one it plays with the display is made again
    if state.info ~= nil and (state.info.pad == true) ~= self.pad then
        self.wantsPad = state.info.pad == true
        return false
    end
    local t = self.tiles
    local full = state.mode == 'game'
    local off = state.mode == 'off'
    self:queueMessages(state.messages or {})
    for _, line in ipairs(state.subtitles or {}) do self.subtitles:add(line) end
    local subtitle = self.subtitles:step(dt)
    for _, event in ipairs(state.questTexts or {}) do self.quests:addEvent(event) end
    for _, line in ipairs(state.objectives or {}) do self.quests:addObjective(line) end
    self.shown:batch(function()
        self:showMessage(dt)
        self:showRegion(state.regions or {}, full, dt)
        self:showQuests(full, dt)
        -- The subtitles are in the modes of the game (and of a script that holds the player), not under a window
        self:put(t.subtitles, 'visible', (full and subtitle ~= nil) and 1 or 0)
        if subtitle ~= nil then self:put(t.subtitleText, 'string', subtitle) end
        self:put(t.messages, 'visible', off and 0 or 1)
        for _, tile in ipairs({ t.hitPoints, t.actionPoints, t.rads, t.reticle, t.sneak, t.xp, t.limbs, t.enemy, t.info }) do
            if not full then self:put(tile, 'visible', 0) end
        end
        if not full then return end
        -- While a script holds the player still the display is the executable's mode 0xC: the radiation, the experience and
        -- the messages (Mac round 12AM B5; the other tiles of that mode are not made yet)
        local scripted = state.scripted == true
        self:put(t.hitPoints, 'visible', scripted and 0 or 1)
        self:put(t.actionPoints, 'visible', scripted and 0 or 1)
        if scripted then
            for _, tile in ipairs({ t.reticle, t.sneak, t.limbs, t.enemy, t.info }) do self:put(tile, 'visible', 0) end
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
        self:showInfo(state.info)
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

-- The display, or nil when the file of the game cannot be shown (`pad`: the player plays with the pad). `previous` is the
-- display that this one replaces (the window changed its size): what waits to be said is kept
function module.create(pad, previous)
    local size = ui.screenSize()
    local shown = menuModule.open(FILE, { layer = 'HUD', passive = true, pad = pad == true })
    if not shown then return nil end
    local self = setmetatable({ shown = shown, size = size, cache = {}, queue = previous and previous.queue or {},
        leaving = previous and previous.leaving or nil, pad = pad == true,
        subtitles = previous and previous.subtitles or subtitles.new(),
        region = previous and previous.region or { text = '', alpha = 0, clock = 0, stamp = 0, hold = false },
        quests = previous and previous.quests or questText.new() }, Hud)
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
