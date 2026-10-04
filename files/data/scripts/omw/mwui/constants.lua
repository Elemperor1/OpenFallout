local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')

return {
    textNormalSize = ui._getDefaultFontSize(),
    textHeaderSize = ui._getDefaultFontSize(),
    headerColor = util.color.commaString(core.getGMST("FontColor_color_header")),
    normalColor = util.color.commaString(core.getGMST("FontColor_color_normal")),
    border = 2,
    thickBorder = 4,
    padding = 2,
    whiteTexture = ui.texture { path = 'white' },
}