local util = require('openfallout.util')

local function shallowLayoutCopy(source, target)
    for k in pairs(target) do
        target[k] = nil
    end
    for k, v in pairs(source) do
        target[k] = v
    end
    return target
end

---
-- @type Templates
-- @usage
-- local I = require('openfallout.interfaces')
-- local ui = require('openfallout.ui')
-- local auxUi = require('openfallout_aux.ui')
-- ui.create {
--     template = I.MWUI.templates.textNormal,
--     layer = 'Windows',
--     type = ui.TYPE.Text,
--     props = {
--         text = 'Hello, world!',
--     },
-- }
-- -- important to copy here
-- local myText = auxUi.deepLayoutCopy(I.MWUI.templates.textNormal)
-- myText.props.textSize = 20
-- I.MWUI.templates.textNormal = myText
-- ui.updateAll()

local templatesMeta = {
    __index = function(self, key)
        return self.__templates[key]
    end,
    __newindex = function(self, key, template)
        local target = self.__templates[key]
        if target == template then
            error("Overriding a template with itself")
        else
            shallowLayoutCopy(template, target)
        end
    end,
}

---
-- @module MWUI
-- @context menu|player
-- @usage require('openfallout.interfaces').MWUI
local function TemplateOverrides(templates)
    return setmetatable({
        __templates = util.makeReadOnly(templates),
    }, templatesMeta)
end

---
-- @field [parent=#MWUI] #Templates templates
local templates = {}

---
-- Container that adds padding around its content.
-- @field [parent=#Templates] openfallout.ui#Template padding

---
-- Standard spacing interval
-- @field [parent=#Templates] openfallout.ui#Template interval
require('scripts.omw.mwui.space')(templates)

---
-- Standard rectangular borders
-- @field [parent=#Templates] openfallout.ui#Template borders

---
-- Container wrapping the content with borders
-- @field [parent=#Templates] openfallout.ui#Template box

---
-- Same as box, but with a semi-transparent background
-- @field [parent=#Templates] openfallout.ui#Template boxTransparent

---
-- Same as box, but with a solid background
-- @field [parent=#Templates] openfallout.ui#Template boxSolid

---
-- Expanding vertical line
-- @field [parent=#Templates] openfallout.ui#Template verticalLine

---
-- Expanding horizontal line
-- @field [parent=#Templates] openfallout.ui#Template horizontalLine

---
-- Standard rectangular borders
-- @field [parent=#Templates] openfallout.ui#Template bordersThick

---
-- Container wrapping the content with borders
-- @field [parent=#Templates] openfallout.ui#Template boxThick

---
-- Same as box, but with a semi-transparent background
-- @field [parent=#Templates] openfallout.ui#Template boxTransparentThick

---
-- Same as box, but with a solid background
-- @field [parent=#Templates] openfallout.ui#Template boxSolidThick

---
-- Expanding vertical line
-- @field [parent=#Templates] openfallout.ui#Template verticalLineThick

---
-- Expanding horizontal line
-- @field [parent=#Templates] openfallout.ui#Template horizontalLineThick
require('scripts.omw.mwui.borders')(templates)

---
-- Standard "sand" colored text
-- @field [parent=#Templates] openfallout.ui#Template textNormal

---
-- Header white colored text
-- @field [parent=#Templates] openfallout.ui#Template textHeader

---
-- Standard "sand" colored multiline text
-- @field [parent=#Templates] openfallout.ui#Template textParagraph
require('scripts.omw.mwui.text')(templates)

---
-- Single line text input
-- @field [parent=#Templates] openfallout.ui#Template textEditLine

---
-- Multiline text input
-- @field [parent=#Templates] openfallout.ui#Template textEditBox
require('scripts.omw.mwui.textEdit')(templates)

---
-- Shades its children and makes them uninteractible
-- @field [parent=#Templates] openfallout.ui#Template disabled
require('scripts.omw.mwui.filters')(templates)

---
-- Interface version
-- @field [parent=#MWUI] #number version
local interface = {
    version = 0,
    templates = TemplateOverrides(templates),
}

return {
    interfaceName = "MWUI",
    interface = interface,
}
