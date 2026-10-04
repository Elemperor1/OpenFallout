local util = require('openfallout.util')
local I = require('openfallout.interfaces')

return {
    interfaceName = 'SpellCasting',
    interface = {
        version = 0,
        explodeSpell = function(spellcast, options) end,
        inflict = function(spellcast, target, range) end,
    },
}
