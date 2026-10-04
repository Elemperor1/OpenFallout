local testing = require('testing_util')
local util = require('openfallout.util')
local world = require('openfallout.world')
local core = require('openfallout.core')
local types = require('openfallout.types')

if not core.contentFiles.has('Morrowind.esm') then
    error('This test requires Morrowind.esm')
end

require('global_issues')
require('global_dialogues')
require('global_mwscript')
require('global_regions')
require('global_weather')

return {
    engineHandlers = {
        onUpdate = testing.updateGlobal,
    },
    eventHandlers = testing.globalEventHandlers,
}
