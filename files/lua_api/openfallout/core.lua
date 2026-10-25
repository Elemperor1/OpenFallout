---
-- Defines functions and types that are available in local, global, menu, and load scripts.
-- @context global|menu|local|player|load
-- @module core
-- @usage local core = require('openfallout.core')



---
-- The revision of OpenMW's Lua API. It is an integer that is incremented every time the API is changed. See the actual value at the top of the page.
-- @field [parent=#core] #number API_REVISION

---
-- Terminates the game and quits to the OS. Should be used only for testing purposes. Not available in load scripts.
-- @function [parent=#core] quit

---
-- Send an event to global scripts. Note: in menu scripts, errors if the game is not running (check @{openfallout.menu#menu.getState}.) Not available in load scripts.
-- @function [parent=#core] sendGlobalEvent
-- @param #string eventName
-- @param eventData

---
-- Simulation time in seconds.
-- The number of simulation seconds passed in the game world since starting a new game. Not available in load scripts.
-- @function [parent=#core] getSimulationTime
-- @return #number

---
-- The scale of simulation time relative to real time. Not available in load scripts.
-- @function [parent=#core] getSimulationTimeScale
-- @return #number

---
-- Game time in seconds. Not available in load scripts.
-- @function [parent=#core] getGameTime
-- @return #number

---
-- The scale of game time relative to simulation time. Not available in load scripts.
-- @function [parent=#core] getGameTimeScale
-- @return #number

---
-- Whether the world is paused. Not available in load scripts.
-- @function [parent=#core] isWorldPaused
-- @return #boolean

---
-- Real time in seconds; starting point is not fixed (can be time since last reboot), use only for measuring intervals. For Unix time use `os.time()`. Not available in load scripts.
-- @function [parent=#core] getRealTime
-- @return #number

---
-- Frame duration in seconds. Not available in global or load scripts.
-- @function [parent=#core] getRealFrameDuration
-- @return #number

---
-- Get a game setting with given name (from GMST ESM records or from openmw.cfg). Not available in load scripts.
-- @function [parent=#core] getGMST
-- @param #string setting Setting name
-- @return #any
-- @usage local skillBonus = core.getGMST('fMinorSkillBonus') -- get a numeric GMST from ESM data
-- @usage local jailFormatString = core.getGMST('sNotifyMessage42') -- get a string GMST from ESM data
-- @usage local bloodTextureName = core.getGMST('Blood_Texture_1') -- get a "fallback" parameter value from openmw.cfg (always a string)

---
-- The game's difficulty setting.
-- @function [parent=#core] getGameDifficulty
-- @return #number

---
-- Return l10n formatting function for the given context.
-- Localisation files (containing the message names and translations) should be stored in
-- VFS as files of the form `l10n/<ContextName>/<Locale>.yaml`.
--
-- See [Localisation](../modding/localisation.html) for details of the localisation file structure.
--
-- When calling the l10n formatting function, if no localisation can be found for any of the requested locales then
-- the message key will be returned instead (and formatted, if possible).
-- This makes it possible to use the source strings as message identifiers.
--
-- If you do not use the source string as a message identifier you should instead make certain to include
-- a fallback locale with a complete set of messages.
--
-- @function [parent=#core] l10n
-- @param #string context l10n context; recommended to use the name of the mod.
--                This must match the <ContextName> directory in the VFS which stores the localisation files.
-- @param #string fallbackLocale The source locale containing the default messages
--                               If omitted defaults to "en".
-- @return #function
-- @usage
-- # DataFiles/l10n/MyMod/en.yaml
-- good_morning: 'Good morning.'
-- you_have_arrows: |-
--   {count, plural,
--     one {You have one arrow.}
--     other {You have {count} arrows.}
--   }
-- @usage
-- # DataFiles/l10n/MyMod/de.yaml
-- good_morning: "Guten Morgen."
-- you_have_arrows: |-
--   {count, plural,
--     one {Du hast ein Pfeil.}
--     other {Du hast {count} Pfeile.}
--   }
-- "Hello {name}!": "Hallo {name}!"
-- @usage
-- -- Usage in Lua
-- local myMsg = core.l10n('MyMod', 'en')
-- print( myMsg('good_morning') )
-- print( myMsg('you_have_arrows', {count=5}) )
-- print( myMsg('Hello {name}!', {name='World'}) )


---
-- @{#ContentFiles}: functions working with the list of currently loaded content files.
-- @field [parent=#core] #ContentFiles contentFiles

---
-- Functions working with the list of currently loaded content files.
-- @type ContentFiles
-- @field #list<#string> list The current load order (list of content file names).

---
-- Return the index of a specific content file in the load order (or `nil` if there is no such content file).
-- @function [parent=#ContentFiles] indexOf
-- @param #string contentFile
-- @return #number

---
-- Check if the content file with given name present in the load order.
-- @function [parent=#ContentFiles] has
-- @param #string contentFile
-- @return #boolean

---
-- Construct FormId string from content file name and the index in the file.
-- In ESM3 games (e.g. Morrowind) FormIds are used to reference game objects.
-- In ESM4 games (e.g. Skyrim) FormIds are used both for game objects and as record ids.
-- @function [parent=#core] getFormId
-- @param #string contentFile
-- @param #number index
-- @return #string
-- @usage if obj.recordId == core.getFormId('Skyrim.esm', 0x4d7da) then ... end
-- @usage -- In ESM3 content files (e.g. Morrowind) ids are human-readable strings
-- obj.owner.factionId = 'blades'
-- -- In ESM4 (e.g. Skyrim) ids should be constructed using `core.getFormId`:
-- obj.owner.factionId = core.getFormId('Skyrim.esm', 0x72834)
-- @usage -- local scripts
-- local obj = nearby.getObjectByFormId(core.getFormId('Morrowind.esm', 128964))
-- @usage -- global scripts
-- local obj = world.getObjectByFormId(core.getFormId('Morrowind.esm', 128964))


---
-- Any object that exists in the game world and has a specific location.
-- Player, actors, items, and statics are game objects.
-- @type GameObject
-- @extends #userdata
-- @field #string id The unique id of this object (not record id), can be used as a key in a table.
-- @field #string contentFile Lowercase file name of the content file that defines this object; nil for dynamically created objects.
-- @field #boolean enabled Whether the object is enabled or disabled. Global scripts can set the value. Items in containers or inventories can't be disabled.
-- @field #boolean saveState Whether this object has been modified and will be written to the save game. For objects originating from a content file, a value of false restores the object to its original state after loading. Objects spawned by scripts will not be restored on save load. This field is read-only; global scripts can use @{#GameObject.setSaveState} to change it.
-- @field openfallout.util#Vector3 position Object position.
-- @field #number scale Object scale.
-- @field openfallout.util#Transform rotation Object rotation.
-- @field #Cell startingCell The object's original cell. Returns nil if `cell` of the object is nil.
-- @field openfallout.util#Vector3 startingPosition The object original position
-- @field openfallout.util#Transform startingRotation The object original rotation
-- @field #ObjectOwner owner Ownership information
-- @field #Cell cell The cell where the object currently is. During loading a game and for objects in an inventory or a container `cell` is nil.
-- @field #GameObject parentContainer Container or actor that contains (or has in inventory) this object. It is nil if the object is in a cell.
-- @field #any type Type of the object (one of the tables from the package @{openfallout.types#types}).
-- @field #number count Count (>1 means a stack of objects).
-- @field #string recordId Returns record ID of the object in lowercase.
-- @field #string globalVariable Global Variable associated with this object (read only).


---
-- Object owner information
-- @type ObjectOwner
-- @field #string recordId NPC who owns the object (nil if missing). Global and self scripts can set the value.
-- @field #string factionId Faction who owns the object (nil if missing). Global and self scripts can set the value.
-- @field #number factionRank Rank required to be allowed to pick up the object (`nil` if any rank is allowed). Global and self scripts can set the value.

---
-- Does the object still exist and is available.
-- Returns true if the object exists and loaded, and false otherwise. If false, then every
-- access to the object will raise an error.
-- @function [parent=#GameObject] isValid
-- @param self
-- @return #boolean

---
-- Send a local event to the object.
-- @function [parent=#GameObject] sendEvent
-- @param self
-- @param #string eventName
-- @param eventData

---
-- Activate the object.
-- @function [parent=#GameObject] activateBy
-- @param self
-- @param #GameObject actor The actor who activates the object
-- @usage local self = require('openfallout.self')
-- object:activateBy(self)

---
-- Add a new local script to the object.
-- Can be called only from a global script. Script should be specified in a content
-- file (omwgame/omwaddon/omwscripts) with a CUSTOM flag. Scripts can not be attached to Statics.
-- @function [parent=#GameObject] addScript
-- @param self
-- @param #string scriptPath Path to the script in OpenMW virtual filesystem.
-- @param #table initData (optional) Initialization data to be passed to onInit. If missed then Lua initialization data from content files will be used (if exists for this script).

---
-- Whether a script with given path is attached to this object.
-- Can be called only from a global script.
-- @function [parent=#GameObject] hasScript
-- @param self
-- @param #string scriptPath Path to the script in OpenMW virtual filesystem.
-- @return #boolean

---
-- Removes script that was attached by `addScript`
-- Can be called only from a global script.
-- @function [parent=#GameObject] removeScript
-- @param self
-- @param #string scriptPath Path to the script in OpenMW virtual filesystem.

---
-- Sets the object's scale.
-- Can be called only from a global script.
-- @function [parent=#GameObject] setScale
-- @param self
-- @param #number scale Scale desired in game.

---
-- Makes a character or creature of Fallout 3 and New Vegas follow another object, as a companion does.
-- The follower stays within the distance of the target, walks after it when it goes and runs when it is far behind,
-- whatever its AI packages say, until stopFollowing is called. Does nothing for objects that are not characters or
-- creatures of Fallout. It lasts while the cell of the character is unloaded and loaded again, but is not kept in a save game.
-- Can be called only from a global script.
-- @function [parent=#GameObject] startFollowing
-- @param self
-- @param #GameObject target The object to follow, the player for instance.
-- @param #number distance (optional) How close in game units to stay to the target (default 256, also for 0 or less; at least 128 and at most 1024).

---
-- Stops what startFollowing began, so that the character goes back to its AI packages.
-- Can be called only from a global script.
-- @function [parent=#GameObject] stopFollowing
-- @param self

---
-- Sets whether the object will be written to the save game.
-- For objects originating from a content file, setting this to false restores the object to its original state after loading.
-- Objects spawned by scripts will not be restored on save load.
-- Can be called only from a global script.
-- @function [parent=#GameObject] setSaveState
-- @param self
-- @param #boolean saveState

---
-- Moves the object to given cell and position.
-- Can be called only from a global script.
-- The effect is not immediate: the position will be updated only in the next
-- frame. Can be called only from a global script. Enables object if it was disabled.
-- Can be used to move objects from an inventory or a container to the world.
-- @function [parent=#GameObject] teleport
-- @param self
-- @param #any cellOrName A cell to define the destination worldspace; can be either @{#Cell}, or cell name, or an empty string (empty string means the default exterior worldspace).
-- If the worldspace has multiple cells (i.e. an exterior), the destination cell is calculated using `position`.
-- @param openfallout.util#Vector3 position New position.
-- @param #TeleportOptions options (optional) Either table @{#TeleportOptions} or @{openfallout.util#Transform} rotation.

---
-- Either a table with options or a @{openfallout.util#Vector3} rotation.
-- @type TeleportOptions
-- @field openfallout.util#Transform rotation New rotation; if missing, then the current rotation is used.
-- @field #boolean onGround If true, adjust destination position to the ground.

---
-- Moves an object into a container or an inventory. Enables if was disabled.
-- Can be called only from a global script.
-- @function [parent=#GameObject] moveInto
-- @param self
-- @param #any dest @{#Inventory} or @{#GameObject}
-- @usage item:moveInto(types.Actor.inventory(actor))
-- @usage item:moveInto(types.Container.content(container))
-- @usage item:moveInto(container)

---
-- Removes an object or reduces a stack of objects.
-- Can be called only from a global script.
-- @function [parent=#GameObject] remove
-- @param self
-- @param #number count (optional) the number of items to remove (if not specified then the whole stack)

---
-- Splits a stack of items. Original stack is reduced by `count`. Returns a new stack with `count` items.
-- Can be called only from a global script.
-- @function [parent=#GameObject] split
-- @param self
-- @param #number count The number of items to return.
-- @return #GameObject
-- @usage -- take 50 coins from `money` and put to the container `cont`
-- money:split(50):moveInto(types.Container.content(cont))

---
-- The axis aligned bounding box in world coordinates.
-- @function [parent=#GameObject] getBoundingBox
-- @param self
-- @return openfallout.util#Box

---
-- List of GameObjects. Implements [iterables#List](iterables.html#List) of #GameObject
-- @type ObjectList
-- @list <#GameObject>


---
-- A cell of the game world.
-- @type Cell
-- @field #string name Name of the cell (can be empty string).
-- @field #string displayName Human-readable cell name (takes into account *.cel file localizations). Can be an empty string.
-- @field #string id Unique record ID of the cell, based on cell name for interiors and the worldspace for exteriors, or the formID of the cell for ESM4 cells.
-- @field #string region Region of the cell (can be nil).
-- @field #boolean isExterior Whether the cell is an exterior cell. "Exterior" means grid of cells where the player can seamless walk from one cell to another without teleports. QuasiExterior (interior with sky) is not an exterior.
-- @field #boolean isQuasiExterior (DEPRECATED, use `hasTag("QuasiExterior")`) Whether the cell is a quasi exterior (like interior but with the sky and the weather).
-- @field #number gridX Index of the cell by X (only for exteriors).
-- @field #number gridY Index of the cell by Y (only for exteriors).
-- @field #string worldSpaceId Id of the world space (can be nil).
-- @field #boolean hasWater True if the cell contains water.
-- @field #number waterLevel The water level of the cell. (nil if cell has no water).
-- @field #boolean hasSky True if in this cell sky should be rendered.
-- @field #boolean hasBeenLoaded Whether the cell’s references have been fully loaded. Either by script, the player visiting them, or nearby exterior cells. Once true, it remains true for the current running game.
-- @field #PathGrid pathGrid The cell's @{#PathGrid} if it has one.

---
-- Returns true if the cell has given tag.
-- @function [parent=#Cell] hasTag
-- @param self
-- @param #string tag One of "QuasiExterior", "NoSleep".
-- @return #boolean

---
-- Returns true either if the cell contains the object or if the cell is an exterior and the object is also in an exterior.
-- @function [parent=#Cell] isInSameSpace
-- @param self
-- @param #GameObject object
-- @return #boolean
-- @usage
-- if obj1.cell:isInSameSpace(obj2) then
--     dist = (obj1.position - obj2.position):length()
-- else
--     -- the distance can't be calculated because the coordinates are in different spaces
-- end

---
-- Get all objects of given type from the cell; Only available from global scripts.
-- @function [parent=#Cell] getAll
-- @param self
-- @param type (optional) object type (see @{openfallout.types#types})
-- @return #ObjectList
-- @usage
-- local type = require('openfallout.types')
-- local all = cell:getAll()
-- local weapons = cell:getAll(types.Weapon)

---
-- A cell's path grid marking traversable paths.
-- @type PathGrid

---
-- Get all points in this path grid.
-- @function [parent=#PathGrid] getPoints
-- @param self
-- @return #list<#PathGridPoint> A list of @{#PathGridPoint}s.

---
-- A point in a cell's path grid.
-- @type PathGridPoint
-- @field #boolean autoGenerated True if this node was automatically generated in the editor.
-- @field openfallout.util#Vector3 relativePosition The point's position relative to the cell's origin. An exterior cell's origin is its southwest corner.
-- @field #list<#PathGridPoint> connections A list of points connected to this point.

---
-- @type ActiveSpell
-- @field #string name The spell or item display name
-- @field #string id Record id of the spell or item used to cast the spell
-- @field #GameObject item The enchanted item used to cast the spell, or nil if the spell was not cast from an enchanted item. Note that if the spell was cast for a single-use enchantment such as a scroll, this will be nil.
-- @field #GameObject caster The caster object, or nil if the spell has no defined caster
-- @field #boolean fromEquipment If set, this spell is tied to an equipped item and can only be ended by unequipping the item.
-- @field #boolean temporary If set, this spell effect is temporary and should end on its own. Either after a single application or after its duration has run out.
-- @field #boolean affectsBaseValues If set, this spell affects the base values of affected stats, rather than modifying current values.
-- @field #boolean stackable If set, this spell can be applied multiple times. If not set, the same spell can only be applied once from the same source (where source is determined by caster + item). In vanilla rules, consumables are stackable while spells and enchantments are not.
-- @field #string activeSpellId Uniquely identifies this active spell within the affected actor's list of active spells.
-- @field #list<#ActiveSpellEffect> effects The active effects (@{#ActiveSpellEffect}) of this spell.

---
-- @type ActiveSpellEffect
-- @field #number index Index of this effect within the original list of @{#MagicEffectWithParams} of the spell/enchantment/potion this effect came from.
-- @field #string affectedSkill Optional skill ID
-- @field #string affectedAttribute Optional attribute ID
-- @field #string id Magic effect id
-- @field #string name Localized name of the effect
-- @field #number magnitudeThisFrame The magnitude of the effect in the current frame. This will be a new random number between minMagnitude and maxMagnitude every frame. Or nil if the effect has no magnitude.
-- @field #number minMagnitude The minimum magnitude of this effect, or nil if the effect has no magnitude.
-- @field #number maxMagnitude The maximum magnitude of this effect, or nil if the effect has no magnitude.
-- @field #number duration Total duration in seconds of this spell effect, should not be confused with remaining duration. Or nil if the effect is not temporary.
-- @field #number durationLeft Remaining duration in seconds of this spell effect, or nil if the effect is not temporary.


--- Possible @{#EnchantmentType} values
-- @field [parent=#Magic] #EnchantmentType ENCHANTMENT_TYPE

--- `core.magic.ENCHANTMENT_TYPE`
-- @type EnchantmentType
-- @field #number CastOnce Enchantment can be cast once, destroying the enchanted item.
-- @field #number CastOnStrike Enchantment is cast on strike, if there is enough charge.
-- @field #number CastOnUse Enchantment is cast when used, if there is enough charge.
-- @field #number ConstantEffect Enchantment is always active when equipped.


---
-- @type Enchantment
-- @field #string id Enchantment id
-- @field #number type @{#EnchantmentType}
-- @field #boolean autocalcFlag (DEPRECATED, use isAutocalc) If set, the casting cost should be computed based on the effect list rather than read from the cost field
-- @field #boolean isAutocalc If set, the casting cost should be computed based on the effect list rather than read from the cost field
-- @field #number cost
-- @field #number charge Charge capacity. Should not be confused with current charge.
-- @field #list<#MagicEffectWithParams> effects The effects (@{#MagicEffectWithParams}) of the enchantment
-- @usage -- Getting the enchantment of an arbitrary item, if it has one
-- local function getRecord(item)
--     if item.type and item.type.record then
--         return item.type.record(item)
--     end
--     return nil
-- end
-- local function getEnchantment(item)
--     local record = getRecord(item)
--     if record and record.enchant then
--         return core.magic.enchantments.records[record.enchant]
--     end
--     return nil
-- end


---
-- Inventory of a player/NPC or a content of a container.
-- @type Inventory

---
-- The number of items with the given recordId.
-- @function [parent=#Inventory] countOf
-- @param self
-- @param #string recordId
-- @return #number

---
-- Get all items of the given type from the inventory.
-- @function [parent=#Inventory] getAll
-- @param self
-- @param type (optional) items type (see @{openfallout.types#types})
-- @return #ObjectList
-- @usage
-- local types = require('openfallout.types')
-- local self = require('openfallout.self')
-- local playerInventory = types.Actor.inventory(self.object)
-- local all = playerInventory:getAll()
-- local weapons = playerInventory:getAll(types.Weapon)

---
-- Get first item with the given recordId from the inventory. Returns nil if not found.
-- @function [parent=#Inventory] find
-- @param self
-- @param #string recordId
-- @return #GameObject
-- @usage inventory:find('gold_001')

---
-- Will resolve the inventory, filling it with levelled items if applicable, making its contents permanent. Must be used in a global script.
-- @function [parent=#Inventory] resolve
-- @param self
-- @usage inventory:resolve()

---
-- Checks if the inventory has a resolved item list.
-- @function [parent=#Inventory] isResolved
-- @param self
-- @return #boolean
-- @usage inventory:isResolved()

---
-- Get all items with the given recordId from the inventory.
-- @function [parent=#Inventory] findAll
-- @param self
-- @param #string recordId
-- @return #ObjectList
-- @usage for _, item in ipairs(inventory:findAll('common_shirt_01')) do ... end


--- @{#Land}: Functions for interacting with land data. Not available in load scripts.
-- @field [parent=#core] #Land land

---
-- Get the terrain height at a given location.
-- @function [parent=#Land] getHeightAt
-- @param openfallout.util#Vector3 position
-- @param #any cellOrId cell or cell id in their exterior world space to query
-- @return #number

---
-- Get the terrain texture at a given location. As textures are blended and
-- multiple textures can be at one specific position the texture whose center is
-- closest to the position will be returned.
--
-- @function [parent=#Land] getTextureAt
-- @param openfallout.util#Vector3 position
-- @param #any cellOrId cell or cell id in their exterior world space to query
-- @return #nil, #string Texture path or nil if one isn't defined
-- @return #nil, #string Plugin name or nil if failed to retrieve the texture


--- @{#Magic}: spells and spell effects. Not available in load scripts.
-- @field [parent=#core] #Magic magic


--- Possible @{#SpellRange} values
-- @field [parent=#Magic] #SpellRange RANGE

--- `core.magic.RANGE`
-- @type SpellRange
-- @field #number Self Applied on self
-- @field #number Touch On touch
-- @field #number Target Ranged spell


--- Possible @{#MagicEffectId} values
-- @field [parent=#Magic] #MagicEffectId EFFECT_TYPE

--- `core.magic.EFFECT_TYPE`
-- @type MagicEffectId
-- @field #string WaterBreathing "waterbreathing"
-- @field #string SwiftSwim "swiftswim"
-- @field #string WaterWalking "waterwalking"
-- @field #string Shield "shield"
-- @field #string FireShield "fireshield"
-- @field #string LightningShield "lightningshield"
-- @field #string FrostShield "frostshield"
-- @field #string Burden "burden"
-- @field #string Feather "feather"
-- @field #string Jump "jump"
-- @field #string Levitate "levitate"
-- @field #string SlowFall "slowfall"
-- @field #string Lock "lock"
-- @field #string Open "open"
-- @field #string FireDamage "firedamage"
-- @field #string ShockDamage "shockdamage"
-- @field #string FrostDamage "frostdamage"
-- @field #string DrainAttribute "drainattribute"
-- @field #string DrainHealth "drainhealth"
-- @field #string DrainMagicka "drainmagicka"
-- @field #string DrainFatigue "drainfatigue"
-- @field #string DrainSkill "drainskill"
-- @field #string DamageAttribute "damageattribute"
-- @field #string DamageHealth "damagehealth"
-- @field #string DamageMagicka "damagemagicka"
-- @field #string DamageFatigue "damagefatigue"
-- @field #string DamageSkill "damageskill"
-- @field #string Poison "poison"
-- @field #string WeaknessToFire "weaknesstofire"
-- @field #string WeaknessToFrost "weaknesstofrost"
-- @field #string WeaknessToShock "weaknesstoshock"
-- @field #string WeaknessToMagicka "weaknesstomagicka"
-- @field #string WeaknessToCommonDisease "weaknesstocommondisease"
-- @field #string WeaknessToBlightDisease "weaknesstoblightdisease"
-- @field #string WeaknessToCorprusDisease "weaknesstocorprusdisease"
-- @field #string WeaknessToPoison "weaknesstopoison"
-- @field #string WeaknessToNormalWeapons "weaknesstonormalweapons"
-- @field #string DisintegrateWeapon "disintegrateweapon"
-- @field #string DisintegrateArmor "disintegratearmor"
-- @field #string Invisibility "invisibility"
-- @field #string Chameleon "chameleon"
-- @field #string Light "light"
-- @field #string Sanctuary "sanctuary"
-- @field #string NightEye "nighteye"
-- @field #string Charm "charm"
-- @field #string Paralyze "paralyze"
-- @field #string Silence "silence"
-- @field #string Blind "blind"
-- @field #string Sound "sound"
-- @field #string CalmHumanoid "calmhumanoid"
-- @field #string CalmCreature "calmcreature"
-- @field #string FrenzyHumanoid "frenzyhumanoid"
-- @field #string FrenzyCreature "frenzycreature"
-- @field #string DemoralizeHumanoid "demoralizehumanoid"
-- @field #string DemoralizeCreature "demoralizecreature"
-- @field #string RallyHumanoid "rallyhumanoid"
-- @field #string RallyCreature "rallycreature"
-- @field #string Dispel "dispel"
-- @field #string Soultrap "soultrap"
-- @field #string Telekinesis "telekinesis"
-- @field #string Mark "mark"
-- @field #string Recall "recall"
-- @field #string DivineIntervention "divineintervention"
-- @field #string AlmsiviIntervention "almsiviintervention"
-- @field #string DetectAnimal "detectanimal"
-- @field #string DetectEnchantment "detectenchantment"
-- @field #string DetectKey "detectkey"
-- @field #string SpellAbsorption "spellabsorption"
-- @field #string Reflect "reflect"
-- @field #string CureCommonDisease "curecommondisease"
-- @field #string CureBlightDisease "cureblightdisease"
-- @field #string CureCorprusDisease "curecorprusdisease"
-- @field #string CurePoison "curepoison"
-- @field #string CureParalyzation "cureparalyzation"
-- @field #string RestoreAttribute "restoreattribute"
-- @field #string RestoreHealth "restorehealth"
-- @field #string RestoreMagicka "restoremagicka"
-- @field #string RestoreFatigue "restorefatigue"
-- @field #string RestoreSkill "restoreskill"
-- @field #string FortifyAttribute "fortifyattribute"
-- @field #string FortifyHealth "fortifyhealth"
-- @field #string FortifyMagicka "fortifymagicka"
-- @field #string FortifyFatigue "fortifyfatigue"
-- @field #string FortifySkill "fortifyskill"
-- @field #string FortifyMaximumMagicka "fortifymaximummagicka"
-- @field #string AbsorbAttribute "absorbattribute"
-- @field #string AbsorbHealth "absorbhealth"
-- @field #string AbsorbMagicka "absorbmagicka"
-- @field #string AbsorbFatigue "absorbfatigue"
-- @field #string AbsorbSkill "absorbskill"
-- @field #string ResistFire "resistfire"
-- @field #string ResistFrost "resistfrost"
-- @field #string ResistShock "resistshock"
-- @field #string ResistMagicka "resistmagicka"
-- @field #string ResistCommonDisease "resistcommondisease"
-- @field #string ResistBlightDisease "resistblightdisease"
-- @field #string ResistCorprusDisease "resistcorprusdisease"
-- @field #string ResistPoison "resistpoison"
-- @field #string ResistNormalWeapons "resistnormalweapons"
-- @field #string ResistParalysis "resistparalysis"
-- @field #string RemoveCurse "removecurse"
-- @field #string TurnUndead "turnundead"
-- @field #string SummonScamp "summonscamp"
-- @field #string SummonClannfear "summonclannfear"
-- @field #string SummonDaedroth "summondaedroth"
-- @field #string SummonDremora "summondremora"
-- @field #string SummonAncestralGhost "summonancestralghost"
-- @field #string SummonSkeletalMinion "summonskeletalminion"
-- @field #string SummonBonewalker "summonbonewalker"
-- @field #string SummonGreaterBonewalker "summongreaterbonewalker"
-- @field #string SummonBonelord "summonbonelord"
-- @field #string SummonWingedTwilight "summonwingedtwilight"
-- @field #string SummonHunger "summonhunger"
-- @field #string SummonGoldenSaint "summongoldensaint"
-- @field #string SummonFlameAtronach "summonflameatronach"
-- @field #string SummonFrostAtronach "summonfrostatronach"
-- @field #string SummonStormAtronach "summonstormatronach"
-- @field #string FortifyAttack "fortifyattack"
-- @field #string CommandCreature "commandcreature"
-- @field #string CommandHumanoid "commandhumanoid"
-- @field #string BoundDagger "bounddagger"
-- @field #string BoundLongsword "boundlongsword"
-- @field #string BoundMace "boundmace"
-- @field #string BoundBattleAxe "boundbattleaxe"
-- @field #string BoundSpear "boundspear"
-- @field #string BoundLongbow "boundlongbow"
-- @field #string ExtraSpell "extraspell"
-- @field #string BoundCuirass "boundcuirass"
-- @field #string BoundHelm "boundhelm"
-- @field #string BoundBoots "boundboots"
-- @field #string BoundShield "boundshield"
-- @field #string BoundGloves "boundgloves"
-- @field #string Corprus "corprus"
-- @field #string Vampirism "vampirism"
-- @field #string SummonCenturionSphere "summoncenturionsphere"
-- @field #string SunDamage "sundamage"
-- @field #string StuntedMagicka "stuntedmagicka"
-- @field #string SummonFabricant "summonfabricant"
-- @field #string SummonWolf "summonwolf"
-- @field #string SummonBear "summonbear"
-- @field #string SummonBonewolf "summonbonewolf"
-- @field #string SummonCreature04 "summoncreature04"
-- @field #string SummonCreature05 "summoncreature05"

--- Possible @{#SpellType} values
-- @field [parent=#Magic] #SpellType SPELL_TYPE

--- `core.magic.SPELL_TYPE`
-- @type SpellType
-- @field #number Spell Normal spell, must be cast and costs mana
-- @field #number Ability Innate ability, always in effect
-- @field #number Blight Blight disease
-- @field #number Disease Common disease
-- @field #number Curse Curse
-- @field #number Power Power, can be used once a day

--- @{#Spells}: Spells
-- @field [parent=#Magic] #Spells spells

---
-- Creates a @{#Spell} without adding it to the world database.
-- Use @{openfallout_world#(world).createRecord} to add the record to the world.
-- @function [parent=#Spells] createRecordDraft
-- @param #Spell spell A Lua table with the fields of a Spell, with an optional field `template` that accepts a @{#Spell} as a base.
-- @return #Spell A strongly typed Spell record.

--- List of all @{#Spell}s.
-- @field [parent=#Spells] #list<#Spell> records A read-only list of all @{#Spell} records in the world database, may be indexed by recordId.
-- Implements [iterables#List](iterables.html#List) of #Spell.
-- @usage local spell = core.magic.spells.records['thunder fist']  -- get by id
-- @usage local spell = core.magic.spells.records[1]  -- get by index
-- @usage -- Print all powers
-- for _, spell in pairs(core.magic.spells.records) do
--     if spell.type == core.magic.SPELL_TYPE.Power then
--         print(spell.name)
--     end
-- end

--- @{#Effects}: Magic Effects
-- @field [parent=#Magic] #Effects effects

--- Map from @{#MagicEffectId} to @{#MagicEffect}
-- @field [parent=#Effects] #map<#number, #MagicEffect> records
-- @usage -- Print all harmful effects
-- for _, effect in pairs(core.magic.effects.records) do
--     if effect.harmful then
--         print(effect.name)
--     end
-- end
-- @usage -- Look up the record of a specific effect and print its icon
-- local mgef = core.magic.effects.records[core.magic.EFFECT_TYPE.Reflect]
-- print('Reflect Icon: '..tostring(mgef.icon))

--- @{#Enchantments}: Enchantments
-- @field [parent=#Magic] #Enchantments enchantments

---
-- Creates an @{#Enchantment} without adding it to the world database.
-- Use @{openfallout_world#(world).createRecord} to add the record to the world.
-- @function [parent=#Enchantments] createRecordDraft
-- @param #Enchantment enchantment A Lua table with the fields of an Enchantment, with an optional field `template` that accepts an @{#Enchantment} as a base.
-- @return #Enchantment A strongly typed Enchantment record.

--- A read-only list of all @{#Enchantment} records in the world database, may be indexed by recordId.
-- Implements [iterables#List](iterables.html#List) and [iterables#Map](iterables.html#map-iterable) of #Enchantment.
-- @field [parent=#Enchantments] #list<#Enchantment> records
-- @usage local enchantment = core.magic.enchantments.records['marara's boon']  -- get by id
-- @usage local enchantment = core.magic.enchantments.records[1]  -- get by index
-- @usage -- Print all enchantments with constant effect
-- for _, ench in pairs(core.magic.enchantments.records) do
--     if ench.type == core.magic.ENCHANTMENT_TYPE.ConstantEffect then
--         print(ench.id)
--     end
-- end


---
-- @type Spell
-- @field #string id Spell id
-- @field #string name Spell name
-- @field #number type @{#SpellType}
-- @field #number cost
-- @field #list<#MagicEffectWithParams> effects The effects (@{#MagicEffectWithParams}) of the spell
-- @field #boolean alwaysSucceedFlag If set, the spell should ignore skill checks and always succeed.
-- @field #boolean starterSpellFlag If set, the spell can be selected as a player's starting spell.
-- @field #boolean autocalcFlag (DEPRECATED, use isAutocalc) If set, the casting cost should be computed based on the effect list rather than read from the cost field
-- @field #boolean isAutocalc If set, the casting cost should be computed based on the effect list rather than read from the cost field

---
-- @type MagicEffect
-- @field #string id Effect ID
-- @field #string icon Effect Icon Path
-- @field #string name Localized name of the effect
-- @field #string description Localized description of the effect
-- @field #string school Skill ID that is this effect's school
-- @field #number baseCost
-- @field openfallout.util#Color color
-- @field #boolean harmful If set, the effect is considered harmful and should elicit a hostile reaction from affected NPCs.
-- @field #boolean continuousVfx Whether the magic effect's vfx should loop or not
-- @field #boolean hasDuration If set, the magic effect has a duration. As an example, divine intervention has no duration while fire damage does.
-- @field #boolean hasMagnitude If set, the magic effect depends on a magnitude. As an example, cure common disease has no magnitude while chameleon does.
-- @field #boolean isAppliedOnce If set, the magic effect is applied fully on cast, rather than being continuously applied over the effect's duration. For example, chameleon is applied once, while fire damage is continuously applied for the duration.
-- @field #boolean casterLinked If set, it is implied the magic effect links back to the caster in some way and should end immediately or never be applied if the caster dies or is not an actor.
-- @field #boolean nonRecastable If set, this effect cannot be re-applied until it has ended. This is used by bound equipment spells.
-- @field #string particle Identifier of the particle texture
-- @field #string castStatic Identifier of the vfx static used for casting
-- @field #string hitStatic Identifier of the vfx static used on hit
-- @field #string areaStatic Identifier of the vfx static used for AOE spells
-- @field #string bolt Identifier of the projectile used for ranged spells
-- @field #string castSound Identifier of the sound used for casting
-- @field #string hitSound Identifier of the sound used on hit
-- @field #string areaSound Identifier of the sound used for AOE spells
-- @field #string boltSound Identifier of the projectile sound used for ranged spells
-- @field #boolean hasAttribute True if the effect requires an attribute parameter
-- @field #boolean hasSkill True if the effect requires a skill parameter
-- @field #boolean onSelf True if the effect can be cast on self
-- @field #boolean onTouch True if the effect can be cast on touch
-- @field #boolean onTarget True if the effect can be cast on target
-- @field #boolean unreflectable True if the effect cannot be reflected
-- @field #boolean allowsSpellmaking True if the effect is available for spellmaking
-- @field #boolean allowsEnchanting True if the effect is available for enchanting
-- @field #boolean negativeLight True if the effect casts negative light
-- @field #number speed Projectile speed


---
-- @type MagicEffectWithParams
-- @field #MagicEffect effect @{#MagicEffect}
-- @field #string id ID of the associated @{#MagicEffect}
-- @field #string affectedSkill Optional skill ID
-- @field #string affectedAttribute Optional attribute ID
-- @field #number range
-- @field #number area
-- @field #number magnitudeMin
-- @field #number magnitudeMax
-- @field #number duration
-- @field #number index Index of this effect within the original list of @{#MagicEffectWithParams} of the spell/enchantment/potion this effect came from.

---
-- @type ActiveEffect
-- Magic effect that is currently active on an actor.
-- @field #string affectedSkill Optional skill ID
-- @field #string affectedAttribute Optional attribute ID
-- @field #string id Effect id string
-- @field #string name Localized name of the effect
-- @field #number magnitude Current magnitude of the effect. Will be set to 0 when the effect is removed or expires.
-- @field #number magnitudeBase
-- @field #number magnitudeModifier

--- @{#Sound}: Sounds and Speech. Not available in load scripts.
-- @field [parent=#core] #Sound sound

---
-- Checks if sound system is enabled (any functions to play sounds are no-ops when it is disabled).
-- It can not be enabled or disabled during runtime.
-- @function [parent=#Sound] isEnabled
-- @return #boolean
-- @usage local enabled = core.sound.isEnabled();

---
-- Play a 3D sound, attached to object
--
-- In local scripts can be used only on self.
-- @function [parent=#Sound] playSound3d
-- @param #string soundId ID of Sound record to play
-- @param #GameObject object Object to which we attach the sound
-- @param #table options An optional table with additional optional arguments. Can contain:
--
--   * `timeOffset` - a floating point number >= 0, to some time (in second) from beginning of sound file (default: 0);
--   * `volume` - a floating point number >= 0, to set a sound volume (default: 1);
--   * `pitch` - a floating point number >= 0, to set a sound pitch (default: 1);
--   * `loop` - a boolean, to set if sound should be repeated when it ends (default: false);
-- @usage local params = {
--    timeOffset=0.1,
--    volume=0.3,
--    loop=false,
--    pitch=1.0
-- };
-- core.sound.playSound3d("shock bolt", object, params)

---
-- Play a 3D sound file, attached to object
--
-- In local scripts can be used only on self.
-- @function [parent=#Sound] playSoundFile3d
-- @param #string fileName Path to sound file in VFS
-- @param #GameObject object Object to which we attach the sound
-- @param #table options An optional table with additional optional arguments. Can contain:
--
--   * `timeOffset` - a floating point number >= 0, to some time (in second) from beginning of sound file (default: 0);
--   * `volume` - a floating point number >= 0, to set a sound volume (default: 1);
--   * `pitch` - a floating point number >= 0, to set a sound pitch (default: 1);
--   * `loop` - a boolean, to set if sound should be repeated when it ends (default: false);
-- @usage local params = {
--    timeOffset=0.1,
--    volume=0.3,
--    loop=false,
--    pitch=1.0
-- };
-- core.sound.playSoundFile3d("Sound\\test.mp3", object, params)

---
-- Stop a 3D sound, attached to object
--
-- In local scripts can be used only on self.
-- @function [parent=#Sound] stopSound3d
-- @param #string soundId ID of Sound record to stop
-- @param #GameObject object Object on which we want to stop sound
-- @usage core.sound.stopSound("shock bolt", object);

---
-- Stop a 3D sound file, attached to object
--
-- In local scripts can be used only on self.
-- @function [parent=#Sound] stopSoundFile3d
-- @param #string fileName Path to sound file in VFS
-- @param #GameObject object Object on which we want to stop sound
-- @usage core.sound.stopSoundFile("Sound\\test.mp3", object);

---
-- Check if a sound is playing on the given object
-- @function [parent=#Sound] isSoundPlaying
-- @param #string soundId ID of Sound record to check
-- @param #GameObject object Object on which we want to check sound
-- @return #boolean
-- @usage local isPlaying = core.sound.isSoundPlaying("shock bolt", object);

---
-- Check if a sound file is playing on the given object
-- @function [parent=#Sound] isSoundFilePlaying
-- @param #string fileName Path to sound file in VFS
-- @param #GameObject object Object on which we want to check sound
-- @return #boolean
-- @usage local isPlaying = core.sound.isSoundFilePlaying("Sound\\test.mp3", object);

---
-- Play an animated voiceover.
-- In local scripts can be used only on self.
-- @function [parent=#Sound] say
-- @param #string fileName Path to sound file in VFS
-- @param #GameObject object Object on which we want to play an animated voiceover
-- @param #string text Subtitle text (optional)
-- @usage -- play voiceover for object and print messagebox
-- core.sound.say("Sound\\Vo\\Misc\\voice.mp3", object, "Subtitle text")
-- @usage -- play voiceover for object, without messagebox
-- core.sound.say("Sound\\Vo\\Misc\\voice.mp3", object)

---
-- Stop an animated voiceover
--
-- In local scripts can be used only on self.
-- @function [parent=#Sound] stopSay
-- @param #string fileName Path to sound file in VFS
-- @param #GameObject object Object on which we want to stop an animated voiceover
-- @usage core.sound.stopSay(object);

---
-- Check if an animated voiceover is playing
-- @function [parent=#Sound] isSayActive
-- @param #GameObject object Object on which we want to check an animated voiceover
-- @return #boolean
-- @usage local isActive = isSayActive(object);

---
-- @type SoundRecord
-- @field #string id Sound id
-- @field #string fileName Normalized path to sound file in VFS
-- @field #number volume Raw sound volume, from 0 to 255
-- @field #number minRange Raw minimal range value, from 0 to 255
-- @field #number maxRange Raw maximal range value, from 0 to 255

--- List of all @{#SoundRecord}s.
-- @field [parent=#Sound] #list<#SoundRecord> records A read-only list of all @{#SoundRecord}s in the world database, may be indexed by recordId.
-- Implements [iterables#List](iterables.html#List) of #SoundRecord.
-- @usage local sound = core.sound.records['Ashstorm']  -- get by id
-- @usage local sound = core.sound.records[1]  -- get by index
-- @usage -- Print all sound files paths
-- for _, sound in pairs(core.sound.records) do
--     print(sound.fileName)
-- end

--- @{#Stats}: stats. Not available in load scripts.
-- @field [parent=#core] #Stats stats


--- @{#Attribute} functions
-- @field [parent=#Stats] #Attribute Attribute

--- `core.stats.Attribute`
-- @type Attribute
-- @field #list<#AttributeRecord> records A read-only list of all @{#AttributeRecord}s in the world database, may be indexed by recordId.
-- Implements [iterables#List](iterables.html#List) of #AttributeRecord.
-- @usage local record = core.stats.Attribute.records['example_recordid']
-- @usage local record = core.stats.Attribute.records[1]

---
-- Returns a read-only @{#AttributeRecord}
-- @function [parent=#Attribute] record
-- @param #string recordId
-- @return #AttributeRecord

--- @{#Skill} functions
-- @field [parent=#Stats] #Skill Skill

--- `core.stats.Skill`
-- @type Skill
-- @field #list<#SkillRecord> records A read-only list of all @{#SkillRecord}s in the world database, may be indexed by recordId.
-- Implements [iterables#List](iterables.html#List) of #SkillRecord.
-- @usage local record = core.stats.Skill.records['example_recordid']
-- @usage local record = core.stats.Skill.records[1]

---
-- Returns a read-only @{#SkillRecord}
-- @function [parent=#Skill] record
-- @param #string recordId
-- @return #SkillRecord

---
-- @type AttributeRecord
-- @field #string id Record id
-- @field #string name Human-readable name
-- @field #string description Human-readable description
-- @field #string icon VFS path to the icon
-- @field #number werewolfValue Value for werewolf players

---
-- @type SkillRecord
-- @field #string id Record id
-- @field #string name Human-readable name
-- @field #string description Human-readable description
-- @field #string icon VFS path to the icon
-- @field #string specialization Skill specialization. Either combat, magic, or stealth.
-- @field #MagicSchoolData school Optional magic school
-- @field #string attribute The id of the skill's governing attribute
-- @field #table skillGain Table of the 4 possible skill gain values. See [SkillProgression#SkillUseType](interface_skill_progression.html#SkillUseType).
-- @field #number werewolfValue Value for werewolf players

---
-- @type MagicSchoolData
-- @field #string name Human-readable name
-- @field #string areaSound VFS path to the area sound
-- @field #string boltSound VFS path to the bolt sound
-- @field #string castSound VFS path to the cast sound
-- @field #string failureSound VFS path to the failure sound
-- @field #string hitSound VFS path to the hit sound
-- @field #number autoCalcMax Maximum number of spells of this school to auto calculate

--- @{#Dialogue}: Dialogue. Not available in load scripts.
-- @field [parent=#core] #Dialogue dialogue

---
-- @{#DialogueRecords} functions for journal (quest) read-only records.
-- @field [parent=#Dialogue] journal
-- @usage --print the name of the record, which is a capitalized version of its id
-- print(core.dialogue.journal.records["ms_fargothring"].name) -- MS_FargothRing
-- @usage --print ids of all journal records
-- for _, journalRecord in pairs(core.dialogue.journal.records) do
--     print(journalRecord.id)
-- end
-- @usage --print quest names for all quests the player has inside a player script
-- for _, quest in pairs(types.Player.quests(self)) do
--     print(quest.id, core.dialogue.journal.records[quest.id].questName)
-- end

---
-- @{#DialogueRecords} functions for topic read-only records.
-- @field [parent=#Dialogue] topic
-- @usage --print ids of all topic records
-- for _, topicRecord in pairs(core.dialogue.topic.records) do
--     print(topicRecord.id)
-- end
-- @usage --print all NPC lines for "vivec"
-- for idx, topicInfo in pairs(core.dialogue.topic.records["vivec"].infos) do
--     print(idx, topicInfo.text)
-- end

---
-- @{#DialogueRecords} functions for voice read-only records.
-- @field [parent=#Dialogue] voice
-- @usage --print ids of all voice records
-- for _, voiceRecord in pairs(core.dialogue.voice.records) do
--     print(voiceRecord.id)
-- end
-- @usage --print all NPC lines & sounds for "flee"
-- for idx, voiceInfo in pairs(core.dialogue.voice.records["flee"].infos) do
--     print(idx, voiceInfo.text, voiceInfo.sound)
-- end

---
-- @{#DialogueRecords} functions for greeting read-only records.
-- @field [parent=#Dialogue] greeting
-- @usage --print ids of all greeting records
-- for _, greetingRecord in pairs(core.dialogue.greeting.records) do
--     print(greetingRecord.id)
-- end
-- @usage --print all NPC lines for "greeting 0"
-- for idx, greetingInfo in pairs(core.dialogue.greeting.records["greeting 0"].infos) do
--     print(idx, greetingInfo.text)
-- end

---
-- @{#DialogueRecords} functions for persuasion read-only records.
-- @field [parent=#Dialogue] persuasion
-- @usage --print ids of all persuasion records
-- for _, persuasionRecord in pairs(core.dialogue.persuasion.records) do
--     print(persuasionRecord.id)
-- end
-- @usage --print all NPC lines for "admire success"
-- for idx, persuasionInfo in pairs(core.dialogue.persuasion.records["admire success"].infos) do
--     print(idx, persuasionInfo.text)
-- end

---
-- A read-only list of all @{#DialogueRecord}s in the world database, may be indexed by recordId, which doesn't have to be lowercase.
-- Implements [iterables#List](iterables.html#list-iterable) of #DialogueRecord.
-- @field [parent=#DialogueRecords] #list<#DialogueRecord> records
-- @usage local record = core.dialogue.journal.records['ms_fargothring']
-- @usage local record = core.dialogue.journal.records['MS_FargothRing']
-- @usage local record = core.dialogue.journal.records[1]
-- @usage local record = core.dialogue.topic.records[1]
-- @usage local record = core.dialogue.topic.records['background']
-- @usage local record = core.dialogue.greeting.records[1]
-- @usage local record = core.dialogue.greeting.records['greeting 0']
-- @usage local record = core.dialogue.persuasion.records[1]
-- @usage local record = core.dialogue.persuasion.records['admire success']
-- @usage local record = core.dialogue.voice.records[1]
-- @usage local record = core.dialogue.voice.records["flee"]

---
-- @{#Conversation}: the conversation of the player with a character of Fallout 3 or New Vegas. Not available in load
-- scripts. A conversation starts when the player activates a character who has something to say (or a script of the
-- game starts one); the interface that shows it is the window `Dialogue` that `I.UI.registerWindow` replaces.
-- @field [parent=#core] #Conversation conversation

---
-- Functions of the conversation. The conversation is a state that every function returns, as @{#ConversationState}.
-- @type Conversation

---
-- Whether the loaded content has the dialogue of Fallout 3 or New Vegas (topics and responses).
-- @function [parent=#Conversation] available
-- @return #boolean

---
-- The conversation as it is now.
-- @function [parent=#Conversation] get
-- @return #ConversationState

---
-- The player picks a topic of `options`: the character says the response that the topic gives now. Does nothing when
-- no conversation is open, a response is being said or the topic is not among the options.
-- @function [parent=#Conversation] choose
-- @param #string topic The `topic` of an option, as `get` returns it
-- @return #ConversationState

---
-- The speaker says a line of the response that is said with the voice that the data has for it (the file of the line
-- under `sound/voice`, found by the plugin that has the response, the form id of the response and the number of the
-- line). The sound starts with the next frame; follow it with `voice`.
-- @function [parent=#Conversation] say
-- @param #number line The position of the line in `lines`, from 1.
-- @return #boolean False when the data has no voice file for the line.

---
-- How the voice that `say` started goes: `"pending"` (asked for, the frame that plays it has not come), `"playing"`,
-- `"done"` (played to the end or stopped), or `"none"` (nothing was asked for, the line has no voice file, or the sound
-- could not be played: with no sound output a line stays as long as it takes to read, `ConversationLine.duration`).
-- @function [parent=#Conversation] voice
-- @return #string

---
-- Stops the voice that is playing.
-- @function [parent=#Conversation] stopVoice

---
-- The response that is being said is over (the player has read it or it was played): what its end script does is done,
-- and the topics the player can pick next are worked out. A response flagged as the end of the conversation ends it.
-- Does nothing when no response is being said.
-- @function [parent=#Conversation] finish
-- @return #ConversationState

---
-- Ends the conversation, also in the middle of a response.
-- @function [parent=#Conversation] close
-- @return #ConversationState

---
-- What the player is shown of a conversation.
-- @type ConversationState
-- @field #boolean active A conversation is open.
-- @field #boolean speaking A response is being said; `options` is empty until `finish` is called.
-- @field #boolean ended The conversation is over (it was closed or a response ended it).
-- @field #number duration The seconds that the lines being said take to read.
-- @field #string speakerId Form id of the reference of the character who talks, as `core.getFormId` writes it.
-- @field #string speakerName Name of the character who talks.
-- @field #string topic Form id of the topic of the response that is said (empty when none is).
-- @field #string response Form id of the response that is said (empty when none is).
-- @field #list<#ConversationLine> lines The lines of the response that is said, in order.
-- @field #list<#ConversationOption> options The topics the player can pick from.
-- @usage
-- local state = core.conversation.get()
-- if state.active and not state.speaking then
--     for _, option in ipairs(state.options) do print(option.text) end
-- end

---
-- One thing the character says.
-- @type ConversationLine
-- @field #string text The words.
-- @field #number emotion 0 neutral, 1 anger, 2 disgust, 3 fear, 4 sad, 5 happy, 6 surprise, 7 pained.
-- @field #number emotionValue How strong the emotion is.
-- @field #number duration The seconds that the line takes to read, which is how long it stays when no voice plays.

---
-- A topic the player can pick.
-- @type ConversationOption
-- @field #string topic Form id of the topic, which `choose` takes.
-- @field #string text What the list shows: the prompt of the response the topic gives, or else the name of the topic.

---
-- @{#Chargen}: the screens of the making of the player's character in Fallout 3 and New Vegas (the name, the sex and
-- race, the points of SPECIAL, the tagged skills and the traits). Not available in load scripts. A script of the game
-- opens a menu with the commands `ShowNameMenu`, `ShowRaceMenu`, `ShowSPECIALBookMenu`, `ShowLoveTesterMenu`,
-- `ShowChargenMenu` and `ShowTraitMenu`; the screens are shown by a player script, which gives the choices back with
-- the functions here and ends each screen with `finish`.
-- @field [parent=#core] #Chargen chargen

---
-- Functions of the making of the character.
-- @type Chargen

---
-- The screen that is open, nothing when no menu is open. A menu of the game has one or more screens, one after the
-- other.
-- @function [parent=#Chargen] page
-- @return #ChargenPage

---
-- How the character is now, with what the screens list.
-- @function [parent=#Chargen] state
-- @return #ChargenState

---
-- Sets the name. It is cleaned (no control characters, no spaces at the ends) and cut to `nameLimit` bytes.
-- @function [parent=#Chargen] setName
-- @param #string name
-- @return #string The name that was kept.

---
-- Picks the sex.
-- @function [parent=#Chargen] setFemale
-- @param #boolean female

---
-- Picks the race.
-- @function [parent=#Chargen] setRace
-- @param #string race The `id` of one of `ChargenState.races`

---
-- Gives points to an attribute, or with a negative amount takes them back. Nothing changes when the points are not
-- there to give or the value would leave `minimum` and `maximum`.
-- @function [parent=#Chargen] adjust
-- @param #number attribute The `number` of one of `ChargenState.attributes`
-- @param #number delta
-- @return #boolean Whether the attribute changed.

---
-- Tags a skill that is not tagged, or takes the tag off one that is. At most `tagLimit` skills are tagged.
-- @function [parent=#Chargen] toggleTag
-- @param #number skill The `number` of one of `ChargenState.skills`
-- @return #boolean Whether the tag changed.

---
-- Picks a trait that is not picked, or lets one go. At most `traitLimit` traits are picked.
-- @function [parent=#Chargen] toggleTrait
-- @param #string trait The `id` of one of `ChargenState.traits`
-- @return #boolean Whether the pick changed.

---
-- The player is done with the screen that is open: what it picked becomes the character (the attributes and the skills
-- that come from them and the tags, the traits as perks), and the next screen of the menu opens. Nothing happens when
-- no screen is open.
-- @function [parent=#Chargen] finish

---
-- The screen that is open.
-- @type ChargenPage
-- @field #string page `"name"`, `"race"`, `"special"`, `"skills"` or `"traits"`.
-- @field #number serial Another number for each screen that opens, so that two screens of the same page are told apart.
-- @field #list<#number> values The values the command that opened the menu was called with.

---
-- How the character is.
-- @type ChargenState
-- @field #string name
-- @field #boolean female The sex the player picked, and else the one of its record.
-- @field #string race Form id of the race of the player, as picked or else as its record has it.
-- @field #number pointsLeft The points of SPECIAL that are left to give.
-- @field #number minimum The lowest value of an attribute.
-- @field #number maximum The highest value of an attribute.
-- @field #number tagLimit How many skills are tagged.
-- @field #number traitLimit How many traits are picked.
-- @field #number nameLimit The longest name, in bytes.
-- @field #number tagsLeft
-- @field #number traitsLeft
-- @field #list<#ChargenAttribute> attributes The seven attributes of SPECIAL.
-- @field #list<#ChargenSkill> skills The skills the game has, with what each is now.
-- @field #list<#ChargenTrait> traits The traits of the data (none in Fallout 3).
-- @field #list<#ChargenRace> races The races a player can be.

---
-- @type ChargenAttribute
-- @field #number number The number scripts know the actor value by (5 to 11).
-- @field #string name The name the data has for it.
-- @field #string description What the data says of it.
-- @field #number value

---
-- @type ChargenSkill
-- @field #number number The number scripts know the actor value by.
-- @field #string name
-- @field #string description
-- @field #number attribute The number of the attribute that governs the skill.
-- @field #number value What the skill comes to with the attributes and the tag now.
-- @field #boolean tagged

---
-- @type ChargenTrait
-- @field #string id Form id of the perk that is the trait.
-- @field #string name
-- @field #string description
-- @field #boolean picked

---
-- @type ChargenRace
-- @field #string id Form id of the race.
-- @field #string name
-- @field #boolean selected

---
-- @{#MessageBox}: the message box that a script of Fallout 3 or New Vegas has put on the screen (`ShowMessage` of a
-- message that is a box, or has buttons). Not available in load scripts. The player picks a button with a click; a
-- script can pick one too, which the script that showed the box reads with `GetButtonPressed` in the next frame.
-- @field [parent=#core] #MessageBox messageBox

---
-- @type MessageBox

---
-- The box that is open: its text (the name of the message and its words), the buttons the player can pick from in the
-- order the box lists them, and a number that is another for each box that opens. Nothing when no box is open.
-- @function [parent=#MessageBox] current
-- @return #MessageBoxState

---
-- Picks a button of the box that is open, as a click on it would.
-- @function [parent=#MessageBox] press
-- @param #number button The position of the button in `buttons`, from 1.
-- @return #boolean False when no box is open or it has no such button.

---
-- @type MessageBoxState
-- @field #number serial Another number for each box that opens.
-- @field #string form The message record the box shows, written as `core.getFormId` writes a form.
-- @field #string text
-- @field #list<#string> buttons

---
-- @{#Combat}: the fights of the characters of Fallout 3 or New Vegas, as the scripts of the game see them. The
-- characters are told by their form ids, as `core.getFormId` writes one ("" for none). Not available in
-- load scripts.
-- @field [parent=#core] #Combat combat

---
-- @type Combat

---
-- The attacker strikes the victim: the blow is rolled and its damage worked out from the weapon, the skills and the
-- armor, taken from the health of the victim, and the events of the scripts (OnHit, OnHitWith, OnDeath) are raised.
-- @function [parent=#Combat] strike
-- @param #string attacker
-- @param #string victim
-- @param #string weapon The weapon that is used; the one the attacker holds when it is left out or empty, or its hands.
-- @param #number limb The type of the part of the body that is hit (0 torso, 1 and 2 head, 3 and 4 left arm, 5 and 6 right arm, 7 to 9 left leg, 10 to 12 right leg, 13 brain, 14 weapon), -1 (the default) when it is not known.
-- @return #CombatStrike

---
-- Whom the character fights.
-- @function [parent=#Combat] target
-- @param #string actor
-- @return #string The target, "" when the character is at peace.

---
-- @function [parent=#Combat] inCombat
-- @param #string actor
-- @return #boolean

---
-- The characters that fight the target.
-- @function [parent=#Combat] attackers
-- @param #string target
-- @return #list<#string>

---
-- Makes the actor fight the target. Nothing happens when either is dead, or when the actor is unconscious.
-- @function [parent=#Combat] start
-- @param #string actor
-- @param #string target

---
-- Makes the actor stop fighting.
-- @function [parent=#Combat] stop
-- @param #string actor

---
-- @function [parent=#Combat] isDead
-- @param #string actor
-- @return #boolean

---
-- Whether the content is that of Fallout 3 or New Vegas, which has combat to run (it is not for Morrowind's).
-- @function [parent=#Combat] available
-- @return #boolean

---
-- The weapon the player holds and what it allows.
-- @function [parent=#Combat] weapon
-- @return #CombatWeapon

---
-- The player attacks with its weapon at the object that the ray of its aim hit: a round is used up when the weapon
-- needs one, and a character or creature that was hit loses health as for `strike`. Nothing happens while the weapon
-- is not ready again after the last blow or shot.
-- @function [parent=#Combat] fire
-- @param #Object target The object the aim hit, nil when it hit none.
-- @param #number limb The type of the part of the body that is hit (0 torso, 1 and 2 head, 3 and 4 left arm, 5 and 6 right arm, 7 to 9 left leg, 10 to 12 right leg, 13 brain, 14 weapon), -1 (the default) when it is not known.
-- @return #CombatFire

---
-- The player draws the weapon, or puts it away, for the scripts that ask whether it is out (`GetWeaponAnimType`,
-- `IsWeaponOut`).
-- @function [parent=#Combat] setDrawn
-- @param #boolean drawn

---
-- @type CombatWeapon
-- @field #string weapon Form id of the weapon, "" for the hands.
-- @field #boolean melee Whether a blow reaches no further than the weapon, as a shot does.
-- @field #boolean automatic Whether the weapon goes on firing while the button is held.
-- @field #number range How far a blow or shot reaches, in units.
-- @field #number interval The least seconds between two blows or shots.
-- @field #number ammoUse The rounds that a shot uses up.
-- @field #number rounds The rounds the player has for the weapon.

---
-- @type CombatFire
-- @field #boolean fired Whether a blow or shot was made (not while the weapon is not ready again, or out of rounds).
-- @field #boolean outOfAmmo Whether the weapon had no round to use.
-- @field #boolean hit Whether a character or creature was struck.
-- @field #number damage
-- @field #boolean critical
-- @field #boolean killed
-- @field #boolean downed
-- @field #boolean crippled Whether the part of the body that was hit is crippled now.

---
-- @type CombatStrike
-- @field #boolean hit Whether a blow was struck at all.
-- @field #number damage What the blow took from the health of the victim.
-- @field #boolean critical
-- @field #boolean killed
-- @field #boolean downed Whether the victim fell unconscious.
-- @field #boolean crippled Whether the part of the body that was hit is crippled now.

---
-- @{#Hud}: what the head-up display of Fallout 3 and New Vegas shows besides the stats of the player (see
-- @{#Pipboy}.stats): the messages, the radiation, the sneak meter, the crippled limbs, the character in the crosshair
-- and the compass. Not available in load scripts.
-- @field [parent=#core] #Hud hud

---
-- @type Hud

---
-- The messages that were printed since this was last called, oldest first.
-- @function [parent=#Hud] takeMessages
-- @return #list<#HudMessage>

---
-- The text of a note of text, with the `&name;` tokens in it (the key an action is bound to, the name of the player)
-- filled in.
-- @function [parent=#Hud] noteText
-- @param #string note The form id of the note, as `core.getFormId` writes one.
-- @return #string Nothing for a form that is no note or has no text.

---
-- @type HudMessage
-- @field #string text
-- @field #string icon The path of the picture that goes with the text, "" for none.
-- @field #number seconds How long the line stays.

---
-- The meters that are not the stats of the player.
-- @function [parent=#Hud] status
-- @return #HudStatus

---
-- @type HudStatus
-- @field #number rads The radiation of the player.
-- @field #number radsMax The most the radiation meter shows.
-- @field #boolean sneaking Whether the player sneaks (the sneak meter is shown then).
-- @field #string sneak What the characters around make of the player: "hidden", "caution", "detected" or "danger".
-- @field #string sneakText The word the games print for it.
-- @field #boolean weaponDrawn
-- @field #list<#string> crippled The textures (names under Interface\Stats) of the limbs that are crippled.

---
-- The character or creature that an object is, for the bar of the target under the crosshair.
-- @function [parent=#Hud] target
-- @param #Object object
-- @return #HudTarget Nothing for an object that is no character or creature.

---
-- @type HudTarget
-- @field #string name
-- @field #number health
-- @field #number maxHealth
-- @field #boolean dead
-- @field #boolean hostile Whether it fights the player.

---
-- The way the player faces and the things in the window of the compass.
-- @function [parent=#Hud] compass
-- @return #HudCompass

---
-- @type HudCompass
-- @field #number heading Degrees clockwise from north.
-- @field #list<#HudMarker> markers

---
-- @type HudMarker
-- @field #string kind "quest", "landmark" or "enemy".
-- @field #number share Where in the window it is: 0 at the left edge, 1 at the right.
-- @field #number distance How far away it is along the floor.
-- @field #string name

---
-- @{#Pipboy}: what the player's character is and carries, for the screens of the Pip-Boy, the level up and the loot of
-- containers in Fallout 3 or New Vegas. The holders of items are told by their form ids, as `core.getFormId` writes
-- one; the player when it is left out. The words are those of the game settings of the files. Not available in load
-- scripts.
-- @field [parent=#core] #Pipboy pipboy

---
-- @type Pipboy

---
-- Whether the content is that of Fallout 3 or New Vegas.
-- @function [parent=#Pipboy] available
-- @return #boolean

---
-- The form id of the player.
-- @function [parent=#Pipboy] player
-- @return #string

---
-- The text of a game setting of the files, such as `sInventoryWeapons`.
-- @function [parent=#Pipboy] text
-- @param #string name
-- @return #string "" when the files have no such setting.

---
-- The value of a game setting that is a number, such as `fPlayerDeathReloadTime`.
-- @function [parent=#Pipboy] setting
-- @param #string name
-- @return #number Nil when the files have no such setting.

---
-- How the character is now.
-- @function [parent=#Pipboy] stats
-- @return #PipboyStats

---
-- @type PipboyStats
-- @field #number level
-- @field #number experience
-- @field #number levelStart The experience this level started at.
-- @field #number nextLevel The experience the next level needs.
-- @field #number karma
-- @field #string karmaTitle The title that goes with the karma and the level.
-- @field #number health
-- @field #number maxHealth
-- @field #number actionPoints
-- @field #number maxActionPoints
-- @field #number weight What the character carries.
-- @field #number maxWeight What it can carry.
-- @field #number caps
-- @field #number skillPoints The points that can still be spent on skills.
-- @field #number perkPoints The perks that can still be chosen.
-- @field #number armorRating The resistance of what is worn, a whole number, at most what armor can take away.
-- @field #number threshold New Vegas: what the armor worn and the perks take off every blow (0 in Fallout 3).
-- @field #boolean newVegas Whether the content is that of New Vegas, which tells the threshold and the weight and
-- value of a whole stack.

---
-- The seven attributes of SPECIAL, in order.
-- @function [parent=#Pipboy] attributes
-- @return #list<#PipboyValue>

---
-- The skills of the character.
-- @function [parent=#Pipboy] skills
-- @return #list<#PipboyValue>

---
-- @type PipboyValue
-- @field #number number The number of the actor value.
-- @field #string name
-- @field #string description
-- @field #number value What the character has now.
-- @field #number base What it has before anything adds to it.
-- @field #boolean tagged Skills only.
-- @field #string level The word for how good the skill is (skills only).

---
-- The perks of the player with their ranks.
-- @function [parent=#Pipboy] perks
-- @return #list<#PipboyPerk>

---
-- The perks of the level-up list, the ones the player can choose from with a perk point first. The rest are
-- listed too, for the player to see what is coming (`available` is false for them).
-- @function [parent=#Pipboy] offeredPerks
-- @return #list<#PipboyPerk>

---
-- @type PipboyPerk
-- @field #string id
-- @field #string name
-- @field #string description
-- @field #number rank The rank the player has (0 for none).
-- @field #number ranks The highest rank.
-- @field #boolean available False for a perk of the level-up list that cannot be chosen yet.

---
-- The quests the player follows with the objectives it was told about.
-- @function [parent=#Pipboy] quests
-- @return #list<#PipboyQuest>

---
-- @type PipboyQuest
-- @field #string id
-- @field #string name
-- @field #boolean completed
-- @field #list<#PipboyObjective> objectives

---
-- @type PipboyObjective
-- @field #string text
-- @field #boolean completed

---
-- What the holder carries.
-- @function [parent=#Pipboy] items
-- @param #string holder The player when it is left out.
-- @return #list<#PipboyItem>

---
-- @type PipboyItem
-- @field #string id
-- @field #string name
-- @field #number count
-- @field #number weight What one weighs.
-- @field #number value What one is worth.
-- @field #string category The tab it is listed under: "weapons", "apparel", "aid", "misc" or "ammo".
-- @field #boolean equipped Whether it is worn or held.
-- @field #number damage Weapons.
-- @field #number rating Armor.
-- @field #number threshold Armor.
-- @field #boolean key A key: it is on the keyring and in none of the lists.
-- @field #boolean book A book (listed with the aid).
-- @field #boolean hidden Armor that the files keep from the player: no list of the Pip-Boy shows it.
-- @field #boolean usable Whether the player can use it now; ammunition only while the weapon in hand takes it.
-- @field #number cardWeight What one weighs for the player (New Vegas lightens small things with the Pack Rat perk).
-- @field #number cardDamage Weapons held by the player: the damage of one shot (one pellet) with its skill and perks.
-- @field #number pellets How many pellets a shot flies.
-- @field #boolean hasAmmo Weapons: whether the ammunition of the weapon is known.
-- @field #string ammoName Weapons: the name of that ammunition.
-- @field #number clip Weapons: how many rounds the clip takes.
-- @field #number loaded Weapons: the rounds of the ammunition that are in the clip.
-- @field #number reserve Weapons: the rounds of the ammunition that are not.

---
-- The player does with the item what it is for: wears, holds or takes off a weapon, armor or ammunition, uses up an aid
-- item, reads a book.
-- @function [parent=#Pipboy] use
-- @param #string item
-- @return #string "equipped", "unequipped", "used" or "read"; "" when nothing happened.

---
-- The most items that one drop puts on the floor.
-- @field [parent=#Pipboy] #number dropLimit

---
-- The player drops items in front of it.
-- @function [parent=#Pipboy] drop
-- @param #string item
-- @param #number count One by default.
-- @return #number How many were dropped.

---
-- Items go from one holder to another.
-- @function [parent=#Pipboy] transfer
-- @param #string from
-- @param #string to
-- @param #string item
-- @param #number count
-- @return #number How many moved.

---
-- The player opens or closes a container, which the scripts of the container hear of.
-- @function [parent=#Pipboy] containerOpened
-- @param #string container
-- @param #boolean opened

---
-- The menu of recipes that a script showed (`ShowRecipeMenu` of a workbench, a reloading bench or a campfire in New
-- Vegas): the category and its name, a number that is another for each time a menu opens, and the recipes the player may
-- use (those with a perk or a note it lacks are left out).
-- @function [parent=#Pipboy] recipes
-- @return #PipboyRecipes Nil when no menu is open.

---
-- @type PipboyRecipes
-- @field #string category Form id of the category.
-- @field #string name
-- @field #number serial
-- @field #list<#PipboyRecipe> recipes

---
-- @type PipboyRecipe
-- @field #string id
-- @field #string name
-- @field #string subCategory Form id of the sub-category (Aid, Ammo, Weapons, ...), "" for none.
-- @field #string subCategoryName
-- @field #string status "ready", "skill" (the skill is too low) or "items" (an ingredient is short).
-- @field #number skill The number of the skill, -1 for none.
-- @field #number level The level of the skill that is needed.
-- @field #number skillValue The skill of the player.
-- @field #list<#PipboyRecipeItem> ingredients
-- @field #list<#PipboyRecipeItem> outputs

---
-- @type PipboyRecipeItem
-- @field #string id
-- @field #string name
-- @field #number count How many the recipe takes or gives.
-- @field #number have How many the player has.

---
-- The player makes a recipe of the open menu: the ingredients are taken and the results given.
-- @function [parent=#Pipboy] craft
-- @param #string recipe
-- @return #string "made", "skill" or "items" for what is short; "" for a recipe that is not in the menu.

---
-- The player is done with the menu of recipes.
-- @function [parent=#Pipboy] closeRecipes

---
-- The level the player reached whose points have not been handed out to a screen yet, since this was last called.
-- The games show one screen for each level, so there is one level at a time: the next is handed out after
-- `levelUpDone`.
-- @function [parent=#Pipboy] takeLevelUps
-- @return #list<#PipboyLevelUp>

---
-- The player is done with the screen of the level that was handed out last, so the next level the experience
-- reached can be handed out.
-- @function [parent=#Pipboy] levelUpDone

---
-- @type PipboyLevelUp
-- @field #number level
-- @field #number skillPoints The points the level gave.
-- @field #boolean perk Whether it gave a perk to choose.

---
-- Spends skill points on a skill (a skill is at most 100).
-- @function [parent=#Pipboy] spendSkillPoints
-- @param #number skill The number of the skill.
-- @param #number points
-- @return #number How many were spent.

---
-- Takes skill points back from a skill, as the screen of the level up does for the points it gave itself.
-- @function [parent=#Pipboy] refundSkillPoints
-- @param #number skill The number of the skill.
-- @param #number points
-- @return #number How many were taken back.

---
-- Spends the perk point on one of the perks that are offered.
-- @function [parent=#Pipboy] choosePerk
-- @param #string perk
-- @return #boolean False when the perk is not offered or there is no perk point.

---
-- Whether the player is dead.
-- @function [parent=#Pipboy] playerDead
-- @return #boolean

---
-- The game asks whether to load the most recent save, as it does when the player has died.
-- @function [parent=#Pipboy] askReload

---
-- @{#Vats}: V.A.T.S., the targeting of Fallout 3 and New Vegas that stops the game for attacks at parts of the body. The
-- player spends action points on each attack, which hits by a chance that the weapon, the skill, the distance and the
-- part decide. Not available in load scripts.
-- @field [parent=#core] #Vats vats

---
-- @type Vats

---
-- Whether the content is that of Fallout 3 or New Vegas.
-- @function [parent=#Vats] available
-- @return #boolean

---
-- What the weapon and the action points of the player allow.
-- @function [parent=#Vats] state
-- @return #VatsState

---
-- @type VatsState
-- @field #boolean usable Whether V.A.T.S. can be used (the player is alive and awake).
-- @field #boolean open Whether the screen of V.A.T.S. is open (see setOpen).
-- @field #number actionPoints The action points the player has.
-- @field #number maxActionPoints
-- @field #number cost The action points one attack with the weapon costs.
-- @field #number range How far a target can be in V.A.T.S.: the reach of a blow, twice the range of a thrown weapon, the range
-- of a gun only if its record fixes it. `nil` when there is no limit (the chance of a hit falls with the distance instead).
-- @field #string weapon The name of the weapon the player holds, empty for the hands.

---
-- Whether the object is a character or creature that is alive and within the reach of the weapon.
-- @function [parent=#Vats] canTarget
-- @param openfallout.core#GameObject target
-- @return #boolean

---
-- What the screen shows of a target.
-- @function [parent=#Vats] describe
-- @param openfallout.core#GameObject target
-- @return #VatsTargetInfo

---
-- @type VatsTargetInfo
-- @field #string name
-- @field #number distance How far the target is from the player, in units.
-- @field #number health
-- @field #number maxHealth

---
-- The parts of the body of the target that can be aimed at.
-- @function [parent=#Vats] parts
-- @param openfallout.core#GameObject target
-- @return #list<#VatsPart>

---
-- @type VatsPart
-- @field #number index The number of the part, which attack takes.
-- @field #number type The type of the part (0 torso, 1 and 2 head, 3 and 4 left arm, 5 and 6 right arm, 7 to 9 left leg, 10 to 12 right leg, 13 brain, 14 weapon), which is what `GetHitLocation` tells.
-- @field #string name The name of the part in the body part data.
-- @field #string target What V.A.T.S. calls the part.
-- @field #number chance The chance in percent that an attack at the part hits.
-- @field #number condition The condition of the part in percent (0 is crippled).

---
-- The player attacks the part of the target: the action points and a round are spent and the blow is struck when the
-- roll is below the chance. Call it from the main thread, when the player has queued the attack.
-- @function [parent=#Vats] attack
-- @param openfallout.core#GameObject target
-- @param #number part The `index` of a part.
-- @return #VatsAttack

---
-- @type VatsAttack
-- @field #string refused Why nothing was done: "points", "ammo", "range", "dead" or "none"; nil when the attack was made.
-- @field #boolean hit Whether the blow struck.
-- @field #number cost The action points spent.
-- @field #number chance The chance in percent that was rolled against.
-- @field #number damage
-- @field #boolean critical
-- @field #boolean killed
-- @field #boolean downed
-- @field #boolean crippled Whether the part is crippled now.

---
-- The screen of V.A.T.S. tells the engine that it is open (the action points do not come back, and `GetVATSMode`
-- answers 1).
-- @function [parent=#Vats] setOpen
-- @param #boolean open

---
-- Depending on which store this read-only dialogue record is from, it may either be a journal, topic, greeting, persuasion or voice.
-- @type DialogueRecord
-- @field #string id Record identifier
-- @field #string name Same as id, but with upper cases preserved.
-- @field #string questName Non-nil only for journal records with available value. Holds the quest name for this journal entry. Same info may be available under `infos[1].text` as well, but this variable is made for convenience.
-- @field #list<#DialogueRecordInfo> infos A read-only list containing all @{#DialogueRecordInfo}s for this record, in order.
-- @usage local journalId = core.dialogue.journal.records['A2_4_MiloGone'].id -- "a2_4_milogone"
-- @usage local journalName = core.dialogue.journal.records['A2_4_MiloGone'].name -- "A2_4_MiloGone"
-- @usage local questName = core.dialogue.journal.records['A2_4_MiloGone'].questName -- "Mehra Milo and the Lost Prophecies"

---
-- Holds the read-only data for one of many info entries inside a dialogue record. Depending on the type of the dialogue record (journal, topic, greeting, persuasion or voice), it could be, for example, a single journal entry or a NPC dialogue line.
-- @type DialogueRecordInfo
-- @field #string id Identifier for this info entry. Is unique only within the @{#DialogueRecord} it belongs to.
-- @field #string text Text associated with this info entry.
-- @usage --Variable `aa` below is "Congratulations, %PCName. You are now %PCName the %NextPCRank." in vanilla MW:
-- local aa = core.dialogue.topic.records['advancement'].infos[100].text
-- @usage --Variable `bb` below is "sound/vo/a/f/fle_af003.mp3" in vanilla MW:
-- local bb = core.dialogue.voice.records['flee'].infos[149].sound

---
-- Quest stage (same as in @{openfallout_types#PLAYERQuest.stage}) this info entry is associated with.
-- Non-nil only for journal records.
-- @field [parent=#DialogueRecordInfo] #number questStage

---
-- True if this info entry has the "Finished" flag checked.
-- Non-nil only for journal records.
-- @field [parent=#DialogueRecordInfo] #boolean isQuestFinished

---
-- True if this info entry has the "Restart" flag checked.
-- Non-nil only for journal records.
-- @field [parent=#DialogueRecordInfo] #boolean isQuestRestart

---
-- True if this info entry has the "Quest Name" flag checked.
-- Non-nil only for journal records.
-- If true, then the @{#DialogueRecord}, to which this info entry belongs, should have this info entry's @{#DialogueRecordInfo.text} value available in its @{#DialogueRecord.questName}.
-- @field [parent=#DialogueRecordInfo] #boolean isQuestName

---
-- Faction of which the speaker must be a member for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- Can return an empty string - this means that the actor must not be a member of any faction for this filtering to apply.
-- @field [parent=#DialogueRecordInfo] #string filterActorFaction

---
-- Speaker ID allowing for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- @field [parent=#DialogueRecordInfo] #string filterActorId

---
-- Speaker race allowing for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- @field [parent=#DialogueRecordInfo] #string filterActorRace

---
-- Speaker class allowing for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- @field [parent=#DialogueRecordInfo] #string filterActorClass

---
-- Minimum speaker's rank in their faction allowing for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- Rank index starts from 1, matching the value in @{openfallout_types#NPC.getFactionRank}
-- @field [parent=#DialogueRecordInfo] #number filterActorFactionRank

---
-- Cell name prefix of location where the player must be for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- "Prefix" means that the cell's name starting with this value shall pass the filtering. For example: `filterPlayerCell` being "Seyda Neen" does apply to the cell "Seyda Neen, Fargoth's House".
-- @field [parent=#DialogueRecordInfo] #string filterPlayerCell

---
-- Minimum speaker disposition allowing for this info entry to appear.
-- Always nil for journal records. Otherwise is a nonnegative number, with the zero value representing no conditions, i.e. no filtering applied using these criteria.
-- @field [parent=#DialogueRecordInfo] #number filterActorDisposition

---
-- Speaker gender allowing for this info entry to appear: "male" or "female".
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- @field [parent=#DialogueRecordInfo] #string filterActorGender

---
-- Faction of which the player must be a member for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- @field [parent=#DialogueRecordInfo] #string filterPlayerFaction

---
-- Minimum player's rank in their faction allowing for this info entry to appear.
-- Always nil for journal records. Otherwise the nil value represents no conditions, i.e. no filtering applied using these criteria.
-- Rank index starts from 1, matching the value in @{openfallout_types#NPC.getFactionRank}
-- @field [parent=#DialogueRecordInfo] #number filterPlayerFactionRank

---
-- Sound file path for this info entry.
-- Always nil for journal records or if there is no sound set.
-- @field [parent=#DialogueRecordInfo] #string sound

---
-- MWScript (full script text) executed when this info is chosen.
-- Always nil for journal records or if there is no value set.
-- @field [parent=#DialogueRecordInfo] #string resultScript

---
-- A read-only list of @{#DialogueInfoCondition}s.
-- Always nil for journal records.
-- @field [parent=#DialogueRecordInfo] #list<#DialogueInfoCondition> conditions

---
-- @type DialogueInfoCondition
-- @field #DialogueConditionOperator operator The #{#DialogueConditionOperator} to use in the comparison
-- @field #DialogueConditionType type The condition's @{#DialogueConditionType}
-- @field #number value The value to compare to
-- @field #string recordId The record ID to use in the comparison
-- @field #string variableName The name of the global or local mwscript variable to compare to
-- @field #string cellName The cell name to compare to

--- Possible @{#DialogueConditionOperator} values
-- @field [parent=#Dialogue] #DialogueConditionOperator CONDITION_OPERATOR

--- `core.dialogue.CONDITION_OPERATOR`
-- @type DialogueConditionOperator
-- @field #number Equal ==
-- @field #number NotEqual !=
-- @field #number Greater >
-- @field #number GreaterEqual >=
-- @field #number Less <
-- @field #number LessEqual <=

--- Possible @{#DialogueConditionType} values
-- @field [parent=#Dialogue] #DialogueConditionType CONDITION_TYPE

--- `core.dialogue.CONDITION_TYPE`
-- @type DialogueConditionType
-- @field #number FacReactionLowest Lowest faction reaction from the speaker's primary faction to the player's factions
-- @field #number FacReactionHighest Highest faction reaction from the speaker's primary faction to the player's factions
-- @field #number RankRequirement Check whether the player can advance in the speaker's primary faction
-- @field #number Reputation The speaker's reputation
-- @field #number HealthPercent The speaker's health percentage
-- @field #number PcReputation The player's reputation
-- @field #number PcLevel The player's level
-- @field #number PcHealthPercent The player's health percentage
-- @field #number PcMagicka The player's current magicka
-- @field #number PcFatigue The player's current fatigue
-- @field #number PcStrength The player's current strength
-- @field #number PcBlock The player's current block
-- @field #number PcArmorer The player's current armorer
-- @field #number PcMediumArmor The player's current medium armor
-- @field #number PcHeavyArmor The player's current heavy armor
-- @field #number PcBluntWeapon The player's current blunt weapon
-- @field #number PcLongBlade The player's current long blade
-- @field #number PcAxe The player's current axe
-- @field #number PcSpear The player's current spear
-- @field #number PcAthletics The player's current athletics
-- @field #number PcEnchant The player's current enchant
-- @field #number PcDestruction The player's current destruction
-- @field #number PcAlteration The player's current alteration
-- @field #number PcIllusion The player's current illusion
-- @field #number PcConjuration The player's current conjuration
-- @field #number PcMysticism The player's current mysticism
-- @field #number PcRestoration The player's current restoration
-- @field #number PcAlchemy The player's current alchemy
-- @field #number PcUnarmored The player's current unarmored
-- @field #number PcSecurity The player's current security
-- @field #number PcSneak The player's current sneak
-- @field #number PcAcrobatics The player's current acrobatics
-- @field #number PcLightArmor The player's current light armor
-- @field #number PcShortBlade The player's current short blade
-- @field #number PcMarksman The player's current marksman
-- @field #number PcMercantile The player's current mercantile
-- @field #number PcSpeechcraft The player's current speechcraft
-- @field #number PcHandToHand The player's current hand to hand
-- @field #number PcGender The player's gender
-- @field #number PcExpelled Check whether the player has been expelled from the speaker's primary faction
-- @field #number PcCommonDisease Check if the player has a common disease
-- @field #number PcBlightDisease Check if the player has a blight disease
-- @field #number PcClothingModifier Check the combined value of the player's outfit
-- @field #number PcCrimeLevel The player's bounty
-- @field #number SameGender Check if the speaker's gender matches the player's
-- @field #number SameRace Check if the speaker's race matches the player's
-- @field #number SameFaction Check if the player is a member of the speaker's primary faction
-- @field #number FactionRankDifference The difference between the player's rank in the speaker's primary faction and the speaker's
-- @field #number Detected Whether the speaker has detected the player
-- @field #number Alarmed Whether the speaker was alarmed by the player's crime
-- @field #number Choice The choice index
-- @field #number PcIntelligence The player's current intelligence
-- @field #number PcWillpower The player's current willpower
-- @field #number PcAgility The player's current agility
-- @field #number PcSpeed The player's current speed
-- @field #number PcEndurance The player's current endurance
-- @field #number PcPersonality The player's current personality
-- @field #number PcLuck The player's current luck
-- @field #number PcCorprus Whether the player is affected by the Corprus magic effect
-- @field #number Weather Checks the scriptId of the weather in the player's cell
-- @field #number PcVampire Whether the player is affected by the Vampirism magic effect
-- @field #number Level The speaker's level
-- @field #number Attacked Whether the speaker was attacked
-- @field #number TalkedToPc Whether the speaker has talked to the player before
-- @field #number PcHealth The player's current health
-- @field #number CreatureTarget Whether the speaker is targeting a creature
-- @field #number FriendHit The number of times the player has hit the speaker follower
-- @field #number Fight The speaker's current fight
-- @field #number Hello The speaker's current hello
-- @field #number Alarm The speaker's current alarm
-- @field #number Flee The speaker's current flee
-- @field #number ShouldAttack Whether the speaker would start combat with the player
-- @field #number Werewolf Whether the speaker is in werewolf form
-- @field #number PcWerewolfKills The number of werewolves killed by the player
-- @field #number Global A comparison to the @{#DialogueInfoCondition.variableName} global variable
-- @field #number Local A comparison to the speaker's @{#DialogueInfoCondition.variableName} local variable
-- @field #number Journal A comparison to the player's @{#DialogueInfoCondition.recordId} journal index
-- @field #number Item The number of copies of @{#DialogueInfoCondition.recordId} the player is carrying
-- @field #number Dead The number of dead actors of the given @{#DialogueInfoCondition.recordId}
-- @field #number NotId The speaker's recordId should not match @{#DialogueInfoCondition.recordId}
-- @field #number NotFaction The speaker's faction ID should not match @{#DialogueInfoCondition.recordId}
-- @field #number NotClass The speaker's class should not match @{#DialogueInfoCondition.recordId}
-- @field #number NotRace The speaker's race should not match @{#DialogueInfoCondition.recordId}
-- @field #number NotCell The player's cell name should not start with @{#DialogueInfoCondition.cellName}
-- @field #number NotLocal A comparison to the speaker's @{#DialogueInfoCondition.variableName} local variable

--- @{#Regions}: Regions. Not available in load scripts.
-- @field [parent=#core] #Regions regions

---
-- A read-only list of all @{#RegionRecord}s in the world database.
-- @field [parent=#Regions] #list<#RegionRecord> records
-- @usage local record = core.regions.records['bitter coast region']
-- @usage local record = core.regions.records[1]

---
-- Region data record
-- @type RegionRecord
-- @field #string id Region ID
-- @field #string name Region display name
-- @field openfallout.util#Color mapColor Editor map color for this region.
-- @field #string sleepList A leveled creature list used when sleeping outdoors in this region
-- @field #list<#RegionSoundRef> sounds A read-only list of ambient sound references for this region.
-- Each reference includes a chance and a resolved link to the full sound record.
-- @field #table weatherProbabilities A table mapping @{#WeatherRecord.recordId}s to their probability (0–100), should sum to 100.
-- Valid weather ids include:
--   `"clear"`, `"cloudy"`, `"foggy"`, `"overcast"`, `"rain"`, `"thunderstorm"`, `"ashstorm"`, `"blight"`, `"snow"`, `"blizzard"`
-- @usage print(region.weatherProbabilities["rain"])

---
-- Set one weather probability entry for this region at runtime.
-- Throws an error if `weatherId` is not a valid weather id.
-- Value is clamped to the range [0, 100].
-- Multiple calls modify the current runtime weather table.
-- Keep total weather probabilities at 100 after modifications.
--
-- For a full table replacement, zero the current table first, then set the new values.
-- @function [parent=#RegionRecord] setProbability
-- @param self
-- @param #string weatherId Weather id to modify.
-- @param #number value New probability value.
-- @usage region:setProbability('rain', 30)
-- @usage
-- for weatherId, _ in pairs(region.weatherProbabilities) do
--     region:setProbability(weatherId, 0)
-- end
-- region:setProbability('clear', 70)
-- region:setProbability('rain', 30)

---
-- Reset this region's runtime weather probabilities to defaults from the loaded records.
-- @function [parent=#RegionRecord] resetProbability
-- @param self
-- @usage region:resetProbability()

---
-- Region sound reference
-- @type RegionSoundRef
-- @field #string soundId Sound record ID
-- @field #number chance Multiplicative percentage used to determine whether to play the sound

--- @{#Factions}: Factions. Not available in load scripts.
-- @field [parent=#core] #Factions factions

---
-- A read-only list of all @{#FactionRecord}s in the world database.
-- @field [parent=#Factions] #list<#FactionRecord> records
-- @usage local record = core.factions.records['example_recordid']
-- @usage local record = core.factions.records[1]

---
-- Faction data record
-- @type FactionRecord
-- @field #string id Faction id
-- @field #string name Faction name
-- @field #list<#FactionRank> ranks A read-only list containing data for all ranks in the faction, in order.
-- @field #map<#string, #number> reactions A read-only map containing reactions of other factions to this faction.
-- @field #list<#string> attributes A read-only list containing IDs of attributes to advance ranks in the faction.
-- @field #list<#string> skills A read-only list containing IDs of skills to advance ranks in the faction.
-- @field #boolean hidden If true, the faction won't show in the player's skills menu

---
-- Faction rank data record
-- @type FactionRank
-- @field #string name Faction name Rank display name
-- @field #list<#number> attributeValues Attributes values required to get this rank.
-- @field #number primarySkillValue Primary skill value required to get this rank.
-- @field #number favouredSkillValue Secondary skill value required to get this rank.
-- @field #number factionReputation Required amount of faction reputation to reach this rank.
-- @field #number factionReaction (DEPRECATED) Returns the same as factionReputation.

--- @{#MWScripts}: MWScripts. Not available in load scripts.
-- @field [parent=#core] #MWScript mwscripts

---
-- A read-only list of all @{#MWScriptRecord}s in the world database.
-- @field [parent=#MWScripts] #list<#MWScriptRecord> records
-- @usage local record = core.mwscripts.records['example_recordid']
-- @usage local record = core.mwscripts.records[1]

---
-- MWScript data record
-- @type MWScriptRecord
-- @field #string id MWScript id
-- @field #string text MWScript content


--- @{#Weather}: Weather. Not available in load scripts.
-- @field [parent=#core] #Weather weather

--- List of all @{#WeatherRecord}s.
-- @field [parent=#Weather] #list<#WeatherRecord> records A read-only list of all @{#WeatherRecord}s in the world database, may be indexed by recordId.
-- Implements [iterables#List](iterables.html#List) of #WeatherRecord.
-- @usage local weather = core.weather.records['Cloudy']  -- get by id
-- @usage local weather = core.weather.records[1]  -- get by index
-- @usage -- Print all storms
-- for _, weather in pairs(core.weather.records) do
--     if weather.isStorm then
--         print(weather.name)
--     end
-- end

--- `core.weather.MOON_PHASE`
-- @type MOON_PHASE
-- @field [parent=#MOON_PHASE] #number Full
-- @field [parent=#MOON_PHASE] #number WaningGibbous
-- @field [parent=#MOON_PHASE] #number ThirdQuarter
-- @field [parent=#MOON_PHASE] #number WaningCrescent
-- @field [parent=#MOON_PHASE] #number New
-- @field [parent=#MOON_PHASE] #number WaxingCrescent
-- @field [parent=#MOON_PHASE] #number FirstQuarter
-- @field [parent=#MOON_PHASE] #number WaxingGibbous

--- Moon phases.
-- @field [parent=#Weather] #MOON_PHASE MOON_PHASE

---
-- Data for a moon in the current sky.
-- @type Moon
-- @field #string name The moon's name. For Morrowind, "Masser" or "Secunda".
-- @field #number phase One of @{#MOON_PHASE}.
-- @field #number phaseValue MWScript-compatible phase value: 0 new, 1 crescent, 2 quarter, 3 gibbous, or 4 full.
-- @field #number alpha The alpha of the moon between 0 and 1. 0 when the moon is not visible in the sky.

---
-- Get all moons in the current sky.
-- @function [parent=#Weather] getCurrentMoons
-- @param #Cell cell The cell to get moons for.
-- @return #list<#Moon> Can be nil if the cell is inactive or has no sky.

---
-- Get the current weather
-- @function [parent=#Weather] getCurrent
-- @param #Cell cell The cell to get the current weather for
-- @return #WeatherRecord Can be nil if the cell is inactive or has no weather

---
-- Get the next weather if any
-- @function [parent=#Weather] getNext
-- @param #Cell cell The cell to get the next weather for
-- @return #WeatherRecord Can be nil

---
-- Get current weather transition value
-- @function [parent=#Weather] getTransition
-- @param #Cell cell The cell to get the transition value for
-- @return #number Can be nil if the cell is inactive or has no weather

---
-- Change the weather
-- @function [parent=#Weather] changeWeather
-- @param #string regionId
-- @param #WeatherRecord weather The weather to change to

---
-- Get the current direction of the light of the sun.
-- @function [parent=#Weather] getCurrentSunLightDirection
-- @param #Cell cell The cell to get the sun direction for
-- @return openfallout.util#Vector4 Can be nil if the cell is inactive

---
-- Get the current sun visibility taking weather transition into account.
-- @function [parent=#Weather] getCurrentSunVisibility
-- @param #Cell cell The cell to get the sun visibility for
-- @return #number Can be nil if the cell is inactive or has no weather

---
-- Get the current sun percentage taking weather transition into account.
-- @function [parent=#Weather] getCurrentSunPercentage
-- @param #Cell cell The cell to get the sun percentage for
-- @return #number Can be nil if the cell is inactive or has no weather

---
-- Get the current wind speed taking weather transition into account.
-- @function [parent=#Weather] getCurrentWindSpeed
-- @param #Cell cell The cell to get the wind speed for
-- @return #number Can be nil if the cell is inactive or has no weather

---
-- Get the current storm direction taking weather transition into account.
-- @function [parent=#Weather] getCurrentStormDirection
-- @param #Cell cell The cell to get the storm direction for
-- @return openfallout.util#Vector3 Can be nil if the cell is inactive or has no weather

---
-- Weather data
-- @type WeatherRecord
-- @field #string recordId
-- @field #number scriptId Read-only ID used in mwscript and dialogue
-- @field #string name Read-only weather name
-- @field #number windSpeed Affects the angle of falling rain
-- @field #number cloudSpeed
-- @field #string cloudTexture
-- @field #number cloudsMaximumPercent Affects the speed of weather transitions (0, 1]
-- @field #boolean isStorm Controls whether the weather is considered a storm for animation and movement purposes
-- @field openfallout.util#Vector3 stormDirection
-- @field #number glareView Strength of the sun glare [0, 1]
-- @field #number rainSpeed The speed at which rain falls
-- @field #number rainEntranceSpeed The number of seconds between rain particle batches being created
-- @field #string rainEffect Will return nil if weather has no rainEffect
-- @field #number rainMaxRaindrops The maximum number of rain particle batches to create every rainEntranceSpeed
-- @field #number rainDiameter The area around the player to spawn rain in
-- @field #number rainMaxHeight The maximum height relative to the player to spawn rain at
-- @field #number rainMinHeight The minimum height relative to the player to spawn rain at
-- @field #string rainLoopSoundID
-- @field #table thunderSoundID A read-only array containing the recordIds of the thunder sounds
-- @field #string ambientLoopSoundID
-- @field #string particleEffect Will return nil if weather has no particleEffect
-- @field #number distantLandFogFactor
-- @field #number distantLandFogOffset
-- @field openfallout.util#Color sunDiscSunsetColor
-- @field #TimeOfDayInterpolatorFloat landFogDepth
-- @field #TimeOfDayInterpolatorColor skyColor
-- @field #TimeOfDayInterpolatorColor ambientColor
-- @field #TimeOfDayInterpolatorColor fogColor
-- @field #TimeOfDayInterpolatorColor sunColor

---
-- Interpolates numbers for weathers based on time of day
-- @type TimeOfDayInterpolatorFloat
-- @field #number sunrise
-- @field #number sunset
-- @field #number day
-- @field #number night

---
-- Interpolates colors for weathers based on time of day
-- @type TimeOfDayInterpolatorColor
-- @field openfallout.util#Color sunrise
-- @field openfallout.util#Color sunset
-- @field openfallout.util#Color day
-- @field openfallout.util#Color night

return nil
