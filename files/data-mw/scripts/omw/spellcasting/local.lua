local self = require('openfallout.self')
local types = require('openfallout.types')
local I = require('openfallout.interfaces')
local auxUtil = require('openfallout_aux.util')
local common = require('scripts.omw.spellcasting.common')
local Actor = types.Actor

local interface = auxUtil.shallowCopy(I.SpellCasting)
interface.inflict = common.inflict

I.SpellCasting.addApplyMagicEffectsHandler(function(options)
    if Actor.isDeathFinished(self) then return end
    local id = Actor.activeSpells(self):add(options)
    if id then
        -- It takes a frame for a spell to actually be applied, meaning we cannot yet tell
        -- what effects have been resisted, or did not apply, and therefore what VFX need to be rendered/skipped.
        -- so instead of invoking the function directly, we wait a frame using an event
        self:sendEvent('PlayOnHitEffects', {activeSpellId = id})
    end
end)

local function onPlayOnHitEffects(options)
    local aSpell = Actor.activeSpells(self):getByActiveSpellId(options.activeSpellId)
    if aSpell then
        common.playMagicEffects(self, 'hit', aSpell.effects, true)
    end
end

return {
    interfaceName = 'SpellCasting',
    interface = interface,

    eventHandlers = {
        PlayOnHitEffects = onPlayOnHitEffects,
    },
}
