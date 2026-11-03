-- Models of the game drawn behind a menu (core.menuScene): the lock and the bobby pin of the lockpicking screen, the
-- model behind the pause menu, the item of the Pip-Boy. The games draw these inside their menus, after the world and
-- before the 2D tiles; here the scene is drawn to a texture and a widget in a layer of its own, under the windows,
-- shows the texture, so the tiles of the menu file are over it. It is a module of the screens that have a scene, not a
-- script.
--
--   local scene = require('scripts.omw.falloutscene').open('lockpick')
--   core.menuScene.addModel('lockpick', 'lock', 'meshes/terminals/lockinterface01.nif')
--   ...each frame: scene:update()
--   scene:close()
local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')

local v2 = util.vector2

local module = {}

-- The layer of the pictures of scenes: after the HUD, before the windows, and not to be hit by the pointer
local LAYER = 'MenuScenes'
local layerAsked = false

local Scene = {}
Scene.__index = Scene

-- Whether this install draws scenes
function module.available()
    return core.menuScene ~= nil and ui.layers ~= nil
end

-- Makes the scene `name`, as large as the window. What a script says to the scene (core.menuScene.addModel and the like)
-- goes to it in the order it is said, so it can be said at once; the picture is on the screen from the frame after
-- the scene is made (`update`).
function module.open(name)
    if not module.available() then
        return nil
    end
    if ui.layers.indexOf(LAYER) == nil and not layerAsked then
        layerAsked = true
        ui.layers.insertAfter('HUD', LAYER, { interactive = false })
    end
    local size = ui.screenSize()
    core.menuScene.create(name, math.floor(size.x), math.floor(size.y))
    return setmetatable({ name = name, size = size, texture = v2(math.floor(size.x), math.floor(size.y)) }, Scene)
end

-- Each frame the scene is there: once it is made and its layer is, the picture is put on the screen. True when the
-- picture is shown.
function Scene:update()
    if self.closed then
        return false
    end
    if self.element == nil then
        if not core.menuScene.ready(self.name) or ui.layers.indexOf(LAYER) == nil then
            return false
        end
        self.element = ui.create {
            layer = LAYER,
            type = ui.TYPE.Image,
            props = {
                position = v2(0, 0),
                size = self.size,
                -- A texture that was drawn into has its first row at the bottom, a widget's picture its first at the top:
                -- the picture is read from the bottom row up (the height is read as negative, as a mirrored piece of a
                -- menu reads its width)
                resource = ui.texture {
                    path = core.menuScene.texture(self.name),
                    offset = v2(0, self.texture.y),
                    size = v2(self.texture.x, -self.texture.y),
                },
                ignorePointerEvents = true,
            },
        }
        return true
    end
    -- The picture follows the window (the texture keeps the size it was made with)
    local size = ui.screenSize()
    if size.x ~= self.size.x or size.y ~= self.size.y then
        self.size = size
        self.element.layout.props.size = size
        self.element:update()
    end
    return true
end

-- Takes the picture off the screen and the scene out of the game
function Scene:close()
    if self.closed then
        return
    end
    self.closed = true
    if self.element then
        self.element:destroy()
        self.element = nil
    end
    core.menuScene.destroy(self.name)
end

return module
