-- The hot keys of Fallout 3 and New Vegas, as the head-up display of the executables handles them (FalloutNV.exe, the update
-- 0x77DA60 and the class HotKeyWheel 0x701610; Mac rounds 12AN B2 and 12AO B2): the number keys 1 to 8 are eight slots,
-- each holding an item the player put there in the Pip-Boy. A key that is let go quickly uses its item; a key that is held
-- longer than fHotKeyDownTimer (half a second) shows the wheel of the eight slots, with the held one lit. The picture and the
-- name of the item stay on the screen for fHotKeyShowTimer (a second and a half) after a key.
-- This file keeps the timing of the keys; the tiles are the head-up display's (falloutmenuhud.lua).
local module = {}

-- The default of fHotKeyDownTimer (the game's own is read from the settings when it has one) and of fHotKeyShowTimer
module.DOWN_SECONDS = 0.5
module.SHOW_SECONDS = 1.5
module.SLOTS = 8
-- The second key is the ammunition swap of the wheel: it takes no item
module.AMMO_SWAP = 1

-- The picture of an item as the wheel names it: the record's own icon is under the folder of the icons
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

-- `env`: pressed(slot) is true while the key of the slot (counted from 0) is down; slots() the eight rows of
-- `core.pipboy.hotKeys()`; use(id) uses the item and tells what happened ("" when nothing did); sound(name) plays a sound
-- of the game; seconds(name, default) is the value of a setting of the game
function module.new(env)
    local self = setmetatable({ env = env, held = nil, heldFor = 0, wheel = false, showLeft = 0 }, Keys)
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
        selected = self.wheel and self.held or nil,
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

-- A frame: `allowed` is false while a window is open, a script holds the keys, or the player cannot act (the keys are then
-- not looked at, as the executable does, and what was held is dropped)
function Keys:step(dt, allowed)
    if not allowed then
        self.held, self.wheel, self.showLeft, self.preview = nil, false, 0, nil
        self.heldFor = 0
        return nil
    end
    if self.showLeft > 0 then self.showLeft = self.showLeft - dt end
    if self.held ~= nil then
        if self.env.pressed(self.held) then
            self.heldFor = self.heldFor + dt
            if not self.wheel and self.heldFor > self.down then
                self.wheel = true
                self.rows = self.env.slots()
            end
        else
            -- Let go: the item of the key is used, quickly or from the wheel
            local slot = self.held
            self.held, self.heldFor = nil, 0
            local rows = self.wheel and self.rows or self.env.slots()
            local row = rows[slot + 1]
            self.wheel = false
            if row ~= nil and row.id ~= '' then
                local outcome = self.env.use(row.id)
                if outcome == '' and self.env.sound then self.env.sound('UIVATSInsufficientAP') end
                -- The picture stays for a while after the item was used, with the count it has now
                self:loadPreview(slot)
                self.showLeft = self.show
            else
                self.preview = nil
                self.showLeft = 0
            end
        end
    else
        for slot = 0, module.SLOTS - 1 do
            if slot ~= module.AMMO_SWAP and self.env.pressed(slot) then
                self.held, self.heldFor, self.wheel = slot, 0, false
                -- The picture and name of the item come up at once, the item is used when the key is let go
                self:loadPreview(slot)
                self.showLeft = self.show
                break
            end
        end
    end
    return self:view()
end

return module
