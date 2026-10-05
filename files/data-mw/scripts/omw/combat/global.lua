local I = require('openfallout.interfaces')
local auxUtil = require('openfallout_aux.util')
local common = require('scripts.omw.combat.common')
common.registerSettingsGroup()

local interface = auxUtil.shallowCopy(I.Combat)
interface.adjustDamageForArmor = common.adjustDamageForArmor
interface.adjustDamageForDifficulty = common.adjustDamageForDifficulty
interface.getArmorRating = common.getArmorRating
interface.getArmorSkill = common.getArmorSkill
interface.getSkillAdjustedArmorRating = common.getSkillAdjustedArmorRating
interface.getEffectiveArmorRating = common.getEffectiveArmorRating
interface.pickRandomArmor = common.pickRandomArmor

return {
    interfaceName = 'Combat',
    interface = interface
}
