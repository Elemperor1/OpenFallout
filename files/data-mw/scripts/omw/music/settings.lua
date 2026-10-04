local I = require('openfallout.interfaces')
local storage = require('openfallout.storage')

I.Settings.registerPage({
    key = 'OFMusic',
    l10n = 'OFMusic',
    name = 'Music',
    description = 'settingsPageDescription',
})

I.Settings.registerGroup({
    key = "SettingsOMWMusic",
    page = 'OFMusic',
    l10n = 'OFMusic',
    name = 'musicSettings',
    permanentStorage = true,
    order = 0,
    settings = {
        {
            key = 'CombatMusicEnabled',
            renderer = 'checkbox',
            name = 'CombatMusicEnabled',
            description = 'CombatMusicEnabledDescription',
            default = true,
        }
    },
})
