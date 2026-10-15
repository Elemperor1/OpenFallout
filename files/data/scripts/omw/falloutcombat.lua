-- The player's attacks in Fallout 3 and New Vegas: drawing the weapon and using it. The rules (the ammunition a shot uses,
-- how soon the weapon is ready again, what a hit does to a character, the scripts that hear of it) are the engine's
-- (core.combat); this script aims, which is to look where the camera looks, and asks the engine to attack at what the
-- ray of the aim hits. Shots are not flown: they hit at once, as far as the projectile of the weapon reaches. The hands
-- and melee weapons hit what is in reach. There is no animation, sound or crosshair yet.
local core = require('openfallout.core')
local input = require('openfallout.input')
local self = require('openfallout.self')
local async = require('openfallout.async')
local camera = require('openfallout.camera')
local nearby = require('openfallout.nearby')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local Actor = require('openfallout.types').Actor
local Player = require('openfallout.types').Player

local combat = core.combat

-- Morrowind's combat is left alone for content that is not Fallout's
if not combat.available() then
    return {}
end

-- The aim and the attack are this script's, not Morrowind's
I.Controls.overrideCombatControls(true)

local function controlsAllowed()
    return Player.getControlSwitch(self, Player.CONTROL_SWITCH.Controls)
end

local function setDrawn(drawn)
    Actor.setStance(self, drawn and Actor.STANCE.Weapon or Actor.STANCE.Nothing)
    combat.setDrawn(drawn)
end

input.registerTriggerHandler('ToggleWeapon', async:callback(function()
    if not controlsAllowed() then return end
    if Actor.getStance(self) == Actor.STANCE.Weapon then
        setDrawn(false)
    elseif Player.getControlSwitch(self, Player.CONTROL_SWITCH.Fighting) then
        setDrawn(true)
    end
end))

-- The button was pressed since the last frame
local pressed = false
input.registerActionHandler('Use', async:callback(function(value)
    if value then pressed = true end
end))

local center = util.vector2(0.5, 0.5)

-- What the aim hits within the reach of the weapon: the object, and the part of it that was hit, from how high on it
local function aim(range)
    local from = camera.getPosition()
    local to = from + camera.viewportToWorldVector(center) * range
    local result = nearby.castRay(from, to, { ignore = self, collisionType = nearby.COLLISION_TYPE.Default })
    return result.hitObject
end

local function attack()
    local stats = combat.weapon()
    local target = aim(stats.range)
    combat.fire(target)
end

return {
    engineHandlers = {
        onFrame = function(dt)
            local held = input.getBooleanActionValue('Use')
            local pressedNow = pressed
            pressed = false
            if not controlsAllowed() or not Player.getControlSwitch(self, Player.CONTROL_SWITCH.Fighting) then
                return
            end
            if Actor.getStance(self) ~= Actor.STANCE.Weapon then
                return
            end
            if pressedNow or held then
                local stats = combat.weapon()
                -- A weapon that is not automatic fires once for each press
                if pressedNow or stats.automatic then
                    attack()
                end
            end
        end,
    },
}
