-- The items of the game's settings screens (Gameplay, Display, Audio, Controls), as the code of the game builds them for
-- the file options/start_menu.xml (the class StartMenu, read from the executable in rounds 12V, 12W and 12Y): the
-- submenu each item is in, in the order the code builds them, what kind it is (a toggle with a word for each of its
-- values, or a meter with a number of positions), and where its value is kept.
--
-- An item either has an engine setting that it writes (`option`: the engine takes the change at once, as the game's
-- screens apply a change as soon as the row changes) or the value is kept for the item alone, in the section of the
-- permanent storage that the caller gives (`store`), with nothing in the engine reading it yet. `toIndex(value)` and
-- `fromIndex(index)` give the way between the engine's value and the position of the row (0 is the first value of a
-- toggle and the first position of a meter).
local M = {}

-- The positions of a meter: the game's meters have 16 positions (the fades, the brightness, the sensitivity) and the
-- volumes have 26; the value is the position over the last position (the game's code computes `item / (count - 1)`)
local METER = 16
local VOLUME = 26

local function round(value)
    return math.floor(value + 0.5)
end

-- A volume: the position over the last position
local function volume(option)
    return {
        option = option,
        toIndex = function(value) return round(value * (VOLUME - 1)) end,
        fromIndex = function(index) return index / (VOLUME - 1) end,
    }
end

-- A switch of the engine: the second value is "on"
local function switch(option)
    return {
        option = option,
        toIndex = function(value) return value and 1 or 0 end,
        fromIndex = function(index) return index == 1 end,
    }
end

-- The brightness: the game's brightness meter goes from the INI's fGammaMin (the first position) to fGammaMax (the last);
-- the engine's gamma is the inverse of the game's (the game raises the picture to the power of its value, the engine
-- to the power of one over its own)
local GAMMA_MIN, GAMMA_MAX = 1.4, 0.6
local brightness = {
    option = 'gamma',
    toIndex = function(value)
        local game = 1 / math.max(value, 0.01)
        return math.max(0, math.min(METER - 1, round((game - GAMMA_MIN) / (GAMMA_MAX - GAMMA_MIN) * (METER - 1))))
    end,
    fromIndex = function(index)
        return 1 / (GAMMA_MIN + (GAMMA_MAX - GAMMA_MIN) * index / (METER - 1))
    end,
}

-- The sensitivity of the mouse: the game's meter goes from 0.0005 to 0.0100 in 16 steps (fMouseSensitivity = 0.0005 +
-- 0.0095 * position / 15, from the code of the item) and the game's own start is the third position. The engine's
-- camera sensitivity is a multiplier that is 1 for what the game starts from, so the meter keeps the game's scale
local SENSITIVITY_FIRST, SENSITIVITY_STEP, SENSITIVITY_START = 0.0005, 0.0095, 2
local function gameSensitivity(index) return SENSITIVITY_FIRST + SENSITIVITY_STEP * index / (METER - 1) end
local sensitivity = {
    option = 'mouseSensitivity',
    toIndex = function(value)
        local game = value * gameSensitivity(SENSITIVITY_START)
        return math.max(0, math.min(METER - 1, round((game - SENSITIVITY_FIRST) / SENSITIVITY_STEP * (METER - 1))))
    end,
    fromIndex = function(index) return gameSensitivity(index) / gameSensitivity(SENSITIVITY_START) end,
}

local function toggle(key, word, values, default, binding)
    local item = { key = key, word = word, values = values, count = #values, default = default or 0 }
    for name, field in pairs(binding or {}) do item[name] = field end
    return item
end

local function meter(key, word, count, default, binding)
    local item = { key = key, word = word, count = count, default = default or 0 }
    for name, field in pairs(binding or {}) do item[name] = field end
    return item
end

local OFF_ON = { 'sOff', 'sOn' }
-- The four colours of the HUD and of the Pip-Boy, in the order of their values (the game's code builds the words from the
-- last to the first)
local COLOURS = { 'sGreen', 'sBlue', 'sAmber', 'sWhite' }

-- The submenus, in the order of the game's list, with the number the file keeps the centre of their options in
-- (`user1` to `user4` of the menu), and the items in the order the code builds them
M.groups = {
    {
        key = 'Gameplay', word = 'sGameplay', center = 'user1',
        items = {
            -- the words of a toggle are in the order of its values: the game's code builds them from the last to the first
            toggle('killCam', 'sKillCamMode', { 'sKillCamModeNone', 'sKillCamModePlayer', 'sKillCamModeCinematic' }, 2),
            toggle('hardcore', 'sHardcore', OFF_ON, 0),
            toggle('difficulty', 'sDifficulty',
                { 'sLockLevelNameVeryEasy', 'sLockLevelNameEasy', 'sNormal', 'sLockLevelNameHard', 'sLockLevelNameVeryHard' }, 2),
            toggle('saveOnRest', 'sSaveOnRest', OFF_ON, 1),
            toggle('saveOnWait', 'sSaveOnWait', OFF_ON, 1),
            toggle('saveOnTravel', 'sSaveOnTravel', OFF_ON, 1),
            toggle('trueIronSights', 'sTrueIronSights', OFF_ON, 1),
        },
    },
    {
        key = 'Display', word = 'sDisplay', center = 'user2',
        items = {
            meter('brightness', 'sBrightness', METER, 7, brightness),
            meter('hudOpacity', 'sHUDOpacity', METER, METER - 1),
            toggle('hudColor', 'sHUDColor', COLOURS, 2),
            toggle('pipboyColor', 'sPipboyColor', COLOURS, 2),
            -- a higher position is a smaller texture (iTexMipMapSkip), and the first is the largest
            toggle('textureSize', 'sTextureSize', { 'sLarge', 'sMedium', 'sSmall' }, 0),
            meter('actorFade', 'sActorFade', METER, 8),
            meter('itemFade', 'sItemFade', METER, 8),
            meter('objectFade', 'sObjectFade', METER, 8),
            meter('grassFade', 'sGrassFade', METER, 8),
            meter('shadowFade', 'sShadowFade', METER, 8),
            meter('lightFade', 'sLightFade', METER, 8),
            meter('specularityFade', 'sSpecularityFade', METER, 8),
            meter('objectLODFade', 'sObjectLODFade', METER, 8),
            meter('treeLODFade', 'sTreeLODFade', METER, 8),
            toggle('crosshair', 'sCrosshair', OFF_ON, 1, switch('crosshair')),
            toggle('dialogSubtitles', 'sDialogSubtitles', OFF_ON, 1, switch('subtitles')),
            toggle('generalSubtitles', 'sGeneralSubtitles', OFF_ON, 0),
        },
    },
    {
        key = 'Audio', word = 'sAudio', center = 'user3',
        items = {
            meter('masterVolume', 'sMasterVolume', VOLUME, VOLUME - 1, volume('masterVolume')),
            meter('musicVolume', 'sMusicVolume', VOLUME, 15, volume('musicVolume')),
            meter('footstepsVolume', 'sFootstepsVolume', VOLUME, 12, volume('footstepsVolume')),
            meter('voiceVolume', 'sVoiceVolume', VOLUME, 15, volume('voiceVolume')),
            meter('effectsVolume', 'sEffectsVolume', VOLUME, VOLUME - 1, volume('effectsVolume')),
            meter('radioVolume', 'sRadioVolume', VOLUME, 12),
        },
    },
    {
        key = 'Controls', word = 'sControls', center = 'user4',
        items = {
            -- (the second value is "inverted")
            toggle('cameraPitch', 'sCameraPitch', OFF_ON, 0, switch('invertYAxis')),
            meter('mouseSensitivity', 'sMouseSensitivity', METER, SENSITIVITY_START, sensitivity),
            -- the two items of the pad are in the list only while a pad is plugged in (the second value of the first is
            -- "the controller is enabled")
            toggle('disableController', 'sDisableXBoxController', OFF_ON, 1, { pad = true }),
            toggle('rumble', 'sRumble', OFF_ON, 1, { pad = true }),
        },
    },
}

-- The items of a submenu that the screen shows now (`padConnected`: a pad is plugged in)
function M.visible(group, padConnected)
    local shown = {}
    for _, item in ipairs(group.items) do
        if not item.pad or padConnected then shown[#shown + 1] = item end
    end
    return shown
end

local function clamp(item, index)
    return math.max(0, math.min(item.count - 1, index))
end

-- The position of the item now: from the engine for an item that has a setting there, else what the player picked
-- (`store:get(key)`), else the default
function M.read(item, engine, store)
    if item.option and engine then
        local ok, value = pcall(engine.getOption, item.option)
        if ok and value ~= nil then return clamp(item, item.toIndex(value)) end
    end
    local kept = store and store:get(item.key)
    if type(kept) == 'number' then return clamp(item, kept) end
    return item.default
end

-- Put the item at a position: in the engine for an item that has a setting there, and kept in the store (so that the
-- screens show it again, and a setting that the engine has no counterpart of is not lost)
function M.write(item, index, engine, store)
    index = clamp(item, index)
    if store then store:set(item.key, index) end
    if item.option and engine then
        pcall(engine.setOption, item.option, item.fromIndex(index))
    end
    return index
end

-- Which arrows of a row can be used at a position (the file's `user2`: 0 = only the left, 1 = both, 2 = only the right)
function M.arrows(item, index)
    if item.count <= 1 then return 1 end
    if index <= 0 then return 2 end
    if index >= item.count - 1 then return 0 end
    return 1
end

return M
