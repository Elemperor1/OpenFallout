local testing = require('testing_util')
local core = require('openfallout.core')

if not core.contentFiles.has('Morrowind.esm') then
    error('This test requires Morrowind.esm')
end

return {
    engineHandlers = {
        onFrame = testing.makeUpdateMenu(),
    },
    eventHandlers = testing.menuEventHandlers,
}
