-- The hot keys of Fallout 3 and New Vegas, as the head-up display of the executables handles them (FalloutNV.exe, the update
-- 0x77DA60 and the class HotKeyWheel 0x701610; Mac rounds 12AN B2 and 12AO B2): the number keys 1 to 8 are eight slots,
-- each holding an item the player put there in the Pip-Boy. A key that is let go quickly uses its item and shows nothing
-- (Mac round 12BA G3: a press loads the name and the picture but shows neither); a key that is held longer than
-- fHotKeyDownTimer (half a second) shows the wheel of the eight slots, with the held one lit, the picture and the name. They
-- stay on the screen for fHotKeyShowTimer (a second and a half) after the key is let go.
-- With a pad (FalloutNV.exe 0x701BD0, 0x77DA60; Mac round 12AO B2) the four directions of the D-pad choose the slot, eight ways
-- round the wheel: down, down and left, down and right, up and left, up and right, left and right alone are the slots 7, 6, 8,
-- 1, 3, 4 and 5 (up alone would be the slot of the ammunition swap, which is no slot). A new combination has to stay for more
-- than 50 ms to replace the old one, except to or from none. The picture and the name come up at once, a direction held longer
-- than fHotKeyDownTimer shows the wheel, and letting go uses the item of the slot that was chosen.
-- This file keeps the timing of the keys; the tiles are the head-up display's (falloutmenuhud.lua).
local module = {}

-- The default of fHotKeyDownTimer (the game's own is read from the settings when it has one) and of fHotKeyShowTimer
module.DOWN_SECONDS = 0.5
module.SHOW_SECONDS = 1.5
module.SLOTS = 8
-- The second key is the ammunition swap of the wheel: it takes no item
module.AMMO_SWAP = 1

-- The picture of an item as the wheel names it: the record's own icon is under the folder of the icons
-- A new combination of directions has to stay this long (seconds) to replace the one that was chosen
module.PAD_STABLE_SECONDS = 0.05

-- The slot (counted from 0) that a combination of directions chooses, nil for none: `d` has up, down, left and right
function module.padSlot(d)
    if d == nil then return nil end
    local down, up, left, right = d.down == true, d.up == true, d.left == true, d.right == true
    if left and right then return nil end
    if down and not up then
        if left then return 5 end
        if right then return 7 end
        return 6
    elseif up and not down then
        if left then return 0 end
        if right then return 2 end
        -- Up alone is the direction of the ammunition swap: no slot
        return nil
    elseif not up and not down then
        if left then return 3 end
        if right then return 4 end
    end
    return nil
end

function module.iconPath(icon)
    if type(icon) ~= 'string' or icon == '' then return nil end
    if icon:lower():find('^interface') then return icon end
    return 'Interface\\Icons\\' .. icon
end

-- The name of an item for the screen: with the number when there is more than one ("%s (%d)")
function module.itemText(row)
    if row == nil or row.id == nil or row.id == '' then return '' end
    if row.count > 1 then return string.format('%s (%d)', row.name, row.count) end
    return row.name
end

local Keys = {}
Keys.__index = Keys

-- `env`: pressed(slot) is true while the key of the slot (counted from 0) is down; direction() (a pad only) the directions of
-- the D-pad that are down as a table with `up`, `down`, `left` and `right`; slots() the eight rows of
-- `core.pipboy.hotKeys()`; use(id) uses the item and tells what happened ("" when nothing did); sound(name) plays a sound
-- of the game; seconds(name, default) is the value of a setting of the game
function module.new(env)
    local self = setmetatable({ env = env, held = nil, lit = nil, heldFor = 0, wheel = false, showLeft = 0, padMode = false,
        padSelected = nil, padCandidate = nil, padFor = 0 }, Keys)
    self.down = tonumber(env.seconds and env.seconds('fHotKeyDownTimer', module.DOWN_SECONDS)) or module.DOWN_SECONDS
    self.show = tonumber(env.seconds and env.seconds('fHotKeyShowTimer', module.SHOW_SECONDS)) or module.SHOW_SECONDS
    return self
end

-- What the display shows: the picture and the name of the item (`selector`), the wheel and which slot is lit in it, and
-- the rows. Nothing (nil) when there is nothing to show.
function Keys:view()
    if not self.wheel and self.showLeft <= 0 then return nil end
    return {
        selector = self.preview,
        wheel = self.wheel,
        selected = self.wheel and (self.held or self.lit) or nil,
        slots = self.wheel and self.rows or nil,
    }
end

function Keys:loadPreview(slot)
    local rows = self.env.slots()
    self.rows = rows
    local row = rows[slot + 1]
    if row == nil or row.id == '' then
        self.preview = nil
        return nil
    end
    self.preview = { text = module.itemText(row), icon = module.iconPath(row.icon), slot = slot }
    return row
end

-- The slot the D-pad has chosen, with the stability of the executable: a combination that replaces another one stays for
-- more than 50 ms first, a change to or from none is at once
function Keys:choosePad(raw, dt)
    if raw == self.padSelected then
        self.padCandidate, self.padFor = nil, 0
    elseif raw == nil or self.padSelected == nil then
        self.padSelected, self.padCandidate, self.padFor = raw, nil, 0
    elseif raw == self.padCandidate then
        self.padFor = self.padFor + dt
        if self.padFor > module.PAD_STABLE_SECONDS then
            self.padSelected, self.padCandidate, self.padFor = raw, nil, 0
        end
    else
        self.padCandidate, self.padFor = raw, dt
    end
    return self.padSelected
end

-- A frame of the pad: a slot chosen shows the picture and the name (the wheel after the time of a held key); the slot let go
-- of uses its item
function Keys:stepPad(dt, directions)
    local selected = self:choosePad(module.padSlot(directions), dt)
    if selected ~= nil then
        if self.held ~= selected then
            -- The first slot or another slot: its picture and name come up at once
            self.held = selected
            self:loadPreview(selected)
            self.showLeft = self.show
        end
        self.padMode = true
        self.heldFor = self.heldFor + dt
        if not self.wheel and self.heldFor > self.down then
            self.wheel = true
        end
        -- While the wheel is up its rows, and the picture and name of the held slot, are what the slots hold now (a script
        -- can put another item in a slot while it is held)
        if self.wheel then self:loadPreview(selected) end
    elseif self.padMode then
        -- Let go (or none chosen): what the last chosen slot holds is used
        local slot = self.held
        self.held, self.heldFor, self.padMode, self.wheel = nil, 0, false, false
        local row = slot ~= nil and self.env.slots()[slot + 1] or nil
        if row ~= nil and row.id ~= '' then
            local outcome = self.env.use(row.id)
            if outcome == '' and self.env.sound then self.env.sound('UIVATSInsufficientAP') end
            self:loadPreview(slot)
            self.showLeft = self.show
        else
            self.preview = nil
            self.showLeft = 0
        end
    end
end

-- A frame: `allowed` is false while a window is open, a script holds the keys, or the player cannot act (the keys are then
-- not looked at, as the executable does, and what was held is dropped)
function Keys:step(dt, allowed)
    if not allowed then
        self.held, self.wheel, self.showLeft, self.preview = nil, false, 0, nil
        self.heldFor, self.padMode, self.padSelected, self.padCandidate, self.padFor = 0, false, nil, nil, 0
        return nil
    end
    if self.showLeft > 0 then
        self.showLeft = self.showLeft - dt
        -- The wheel goes with the picture, a second and a half after the last frame of a hold or the let-go
        if self.showLeft <= 0 and self.held == nil and not self.padMode then self.wheel, self.lit = false, nil end
    end
    local directions = self.env.direction and self.env.direction() or nil
    if self.padMode or (directions ~= nil and (directions.up or directions.down or directions.left or directions.right)) then
        self:stepPad(dt, directions)
        return self:view()
    end
    if self.held ~= nil then
        if self.env.pressed(self.held) then
            self.heldFor = self.heldFor + dt
            if not self.wheel and self.heldFor > self.down then
                self.wheel = true
            end
            -- While the wheel is up the time of the picture is made new every frame, and its rows are what the slots hold
            -- now (a script can put another item in a slot while the key is held)
            if self.wheel then
                self:loadPreview(self.held)
                self.showLeft = self.show
            end
        else
            -- Let go: the item of the key is used, quickly or from the wheel
            local slot = self.held
            self.held, self.heldFor = nil, 0
            local wasUp = self.wheel
            local row = self.env.slots()[slot + 1]
            local used = row ~= nil and row.id ~= ''
            if used then
                local outcome = self.env.use(row.id)
                if outcome == '' and self.env.sound then self.env.sound('UIVATSInsufficientAP') end
            end
            if wasUp then
                -- The wheel stays, with the slot lit and the item as it is now, until the time of the picture is over
                self.rows = self.env.slots()
                self.lit = slot
                if used then self:loadPreview(slot) else self.preview = nil end
                self.showLeft = self.show
            else
                -- A quick tap showed nothing and shows nothing
                self.wheel, self.lit, self.preview, self.showLeft = false, nil, nil, 0
            end
        end
    else
        for slot = 0, module.SLOTS - 1 do
            if slot ~= module.AMMO_SWAP and self.env.pressed(slot) then
                -- (a wheel that is up from a hold before stays up, with this slot lit)
                self.held, self.heldFor, self.lit = slot, 0, nil
                -- The name and the picture of the item are loaded, but they come up only if the key is held (the item is
                -- used when it is let go)
                self:loadPreview(slot)
                break
            end
        end
    end
    return self:view()
end

return module
