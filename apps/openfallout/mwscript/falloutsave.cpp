#include "falloutscripts.hpp"

#include <algorithm>
#include <functional>

#include <components/debug/debuglog.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/falloutscript/savecharacters.hpp>
#include <components/falloutscript/savescripts.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/saveworld.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/worldmodel.hpp"

#include "falloutsavepending.hpp"

namespace FalloutScript
{
    /// The state that the host itself keeps (the global variables the scripts set, the clock, the controls the player
    /// was refused, the characters that talking activators speak for, the packages that scripts gave)
    template <>
    struct SaveAccess<OFScript::FalloutScripts>
    {
        static void writeFormat(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.u32(saveFormatVersion);
            out.u8(static_cast<std::uint8_t>(host.mGame));
            out.form(host.mPlayer);
        }

        static void readFormat(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::uint32_t version = in.u32();
            if (version > saveFormatVersion)
                throw SaveTooNewError("The state of Fallout in this save", version);
            const std::uint8_t game = in.u8();
            in.form();
            if (game != static_cast<std::uint8_t>(host.mGame))
                Log(Debug::Warning) << "The save was made for the other game of Fallout (" << int(game)
                                    << "): the state of the scripts may not fit";
        }

        static void writeGlobals(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.count(host.mGlobals.size());
            for (const auto& [global, value] : host.mGlobals)
            {
                out.form(global);
                out.f64(value);
            }
            out.i64(host.mCalendarShift);
            // The numbers of the objects that were made are not given again: a number that a save names (the variables
            // of a script) is never one of another object
            const std::lock_guard made(host.mMadeMutex);
            out.u32(host.mMadeCount);
        }

        static void readGlobals(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> global = in.form();
                const double value = in.f64();
                if (global
                    && host.mStore.get<ESM4::GlobalVariable>().search(OFScript::FalloutScripts::refIdOf(*global)))
                    host.mGlobals[*global] = value;
            }
            {
                const std::lock_guard lock(host.mCalendarMutex);
                host.mCalendarShift = in.i64();
            }
            const FormId madeCount = in.u32();
            const std::lock_guard made(host.mMadeMutex);
            host.mMadeCount = std::max(host.mMadeCount, madeCount);
        }

        static void writeControls(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.u32(host.mControlsOff.load());
        }

        static void readControls(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            // The switches of the engine are restored with the input of the saved game; this is what the games have
            // beyond them (the Pip-Boy, rollover, sneaking)
            host.mControlsOff = in.u32() & ((1u << FalloutScript::playerControlCount) - 1);
        }

        static void writeTalking(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            const std::lock_guard lock(host.mTalkingActorMutex);
            out.count(host.mTalkingActors.size());
            for (const auto& [activator, actor] : host.mTalkingActors)
            {
                out.form(activator);
                out.form(actor);
            }
        }

        static void readTalking(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(8);
            const std::lock_guard lock(host.mTalkingActorMutex);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> activator = in.form();
                const std::optional<FormId> actor = in.form();
                if (activator && actor)
                    host.mTalkingActors[*activator] = *actor;
            }
        }

        static void writePackages(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.count(host.mScriptPackages.size());
            for (const auto& [actor, packages] : host.mScriptPackages)
            {
                out.form(actor);
                out.count(packages.size());
                for (const FormId package : packages)
                    out.form(package);
            }
        }

        static void readPackages(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                std::vector<FormId> packages;
                const std::size_t size = in.count(4);
                for (std::size_t j = 0; j < size; ++j)
                    if (const std::optional<FormId> package = in.form())
                        packages.push_back(*package);
                if (actor && !packages.empty())
                    host.mScriptPackages[*actor] = std::move(packages);
            }
        }

        /// What the host keeps beside the rules of the characters: where the scripts of items found them, whose perks
        /// are brought up to date, how much damage the player did to each character it fought, and the levels the
        /// player reached that were not shown yet
        static void writeCharacters(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.count(host.mItemHolders.size());
            for (const auto& [item, holder] : host.mItemHolders)
            {
                out.form(item);
                out.form(holder);
            }
            out.count(host.mPerkOwners.size());
            for (const FormId owner : host.mPerkOwners)
                out.form(owner);
            {
                const std::lock_guard lock(host.mPlayerDamageMutex);
                out.count(host.mPlayerDamage.size());
                for (const auto& [victim, damage] : host.mPlayerDamage)
                {
                    out.form(victim);
                    out.f64(damage);
                }
            }
            const std::lock_guard lock(host.mLevelUpMutex);
            out.count(host.mLevelUps.size());
            for (const FalloutScript::PlayerProgress::LevelUp& level : host.mLevelUps)
            {
                out.i32(level.mLevel);
                out.i32(level.mSkillPoints);
                out.boolean(level.mPerk);
            }
        }

        static void readCharacters(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t holders = in.count(8);
            for (std::size_t i = 0; i < holders; ++i)
            {
                const std::optional<FormId> item = in.form();
                const std::optional<FormId> holder = in.form();
                if (item && holder)
                    host.mItemHolders[*item] = *holder;
            }
            const std::size_t owners = in.count(4);
            for (std::size_t i = 0; i < owners; ++i)
                if (const std::optional<FormId> owner = in.form())
                    host.mPerkOwners.insert(*owner);
            const std::size_t damaged = in.count(12);
            {
                const std::lock_guard lock(host.mPlayerDamageMutex);
                for (std::size_t i = 0; i < damaged; ++i)
                {
                    const std::optional<FormId> victim = in.form();
                    const double damage = in.f64();
                    if (victim)
                        host.mPlayerDamage[*victim] = damage;
                }
            }
            const std::size_t levels = in.count(9);
            const std::lock_guard lock(host.mLevelUpMutex);
            for (std::size_t i = 0; i < levels; ++i)
            {
                FalloutScript::PlayerProgress::LevelUp level;
                level.mLevel = in.i32();
                level.mSkillPoints = in.i32();
                level.mPerk = in.boolean();
                host.mLevelUps.push_back(level);
            }
            // The screen of a level that was not shown is shown as soon as the game asks
            host.mLevelUpOpen = !host.mLevelUps.empty();
        }

        /// The furniture that characters use and the doors that scripts took the collision of
        static void writeFurniture(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            {
                const std::lock_guard lock(host.mFurnitureMutex);
                out.count(host.mFurniture.size());
                for (const auto& [actor, use] : host.mFurniture)
                {
                    out.form(actor);
                    out.form(use.mFurniture);
                    for (int axis = 0; axis < 3; ++axis)
                        out.f32(use.mPlace[axis]);
                    out.i32(use.mEntry);
                    out.i32(use.mKind);
                    for (int axis = 0; axis < 3; ++axis)
                        out.f32(use.mFurniturePlace[axis]);
                }
            }
            out.count(host.mDoorsWithoutCollision.size());
            for (const FormId door : host.mDoorsWithoutCollision)
                out.form(door);
        }

        static void readFurniture(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, OFScript::FalloutScripts::FurnitureUse> uses;
            const std::size_t count = in.count(4 + 4 + 12 + 8 + 12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::optional<FormId> furniture = in.form();
                OFScript::FalloutScripts::FurnitureUse use;
                for (int axis = 0; axis < 3; ++axis)
                    use.mPlace[axis] = in.f32();
                use.mEntry = in.i32();
                use.mKind = in.i32();
                for (int axis = 0; axis < 3; ++axis)
                    use.mFurniturePlace[axis] = in.f32();
                if (actor && furniture)
                {
                    use.mFurniture = *furniture;
                    uses[*actor] = use;
                }
            }
            std::set<FormId> doors;
            const std::size_t doorCount = in.count(4);
            for (std::size_t i = 0; i < doorCount; ++i)
                if (const std::optional<FormId> door = in.form())
                    doors.insert(*door);
            {
                const std::lock_guard lock(host.mFurnitureMutex);
                host.mFurniture = std::move(uses);
            }
            host.mDoorsWithoutCollision = std::move(doors);
        }

        using Place = OFScript::FalloutScripts::Place;
        using Move = OFScript::FalloutScripts::Move;

        /// A cell by its form when it has one (a save follows the plugin to another place in the load order), else by
        /// the text of its id
        static void writeCell(SaveWriter& out, const ESM::RefId& cell)
        {
            const ESM::FormId* form = cell.getIf<ESM::FormId>();
            if (form != nullptr && form->hasContentFile() && form->mContentFile <= 0xfe)
            {
                out.u8(1);
                out.form(form->toUint32());
            }
            else if (!cell.empty())
            {
                out.u8(2);
                out.string(cell.serializeText());
            }
            else
                out.u8(0);
        }

        /// False when the cell is of a plugin that is not loaded now
        static bool readCell(SaveReader& in, ESM::RefId& cell)
        {
            const std::uint8_t kind = in.u8();
            if (kind == 1)
            {
                const std::optional<FormId> form = in.form();
                if (!form)
                    return false;
                cell = OFScript::FalloutScripts::refIdOf(*form);
                return true;
            }
            if (kind == 2)
            {
                cell = ESM::RefId::deserializeText(in.string());
                return true;
            }
            if (kind != 0)
                throw SaveError("A place of the save has a kind of cell that this engine does not know");
            cell = ESM::RefId();
            return true;
        }

        static void writePlace(SaveWriter& out, const Place& place)
        {
            writeCell(out, place.mCell);
            for (int axis = 0; axis < 3; ++axis)
                out.f32(place.mPosition[axis]);
            for (int axis = 0; axis < 3; ++axis)
                out.f32(place.mAngle[axis]);
        }

        /// False when the cell of the place is of a plugin that is not loaded now
        static bool readPlace(SaveReader& in, Place& place)
        {
            const bool known = readCell(in, place.mCell);
            for (int axis = 0; axis < 3; ++axis)
                place.mPosition[axis] = in.f32();
            for (int axis = 0; axis < 3; ++axis)
                place.mAngle[axis] = in.f32();
            return known;
        }

        /// A reference of a content file that something happened to, and a reference or object that was made while the
        /// game ran
        struct Entry
        {
            FormId mReference = 0; // for an object: the number scripts know it by, 0 for none
            FormId mBase = 0; // objects only
            ESM::RefNum mNumber; // objects only: the number that Lua knows it by
            bool mEnabled = true;
            int mCount = 1;
            float mScale = 1;
            Place mWhere;
            std::optional<ESM::LuaScripts> mLua;
        };

        /// The state of the Lua scripts of an object: the scripts (by the number they have in the configuration, which
        /// the save of Lua maps), the data of each, which is binary, and its timers
        static void writeLua(SaveWriter& out, const std::optional<ESM::LuaScripts>& lua)
        {
            out.boolean(lua.has_value());
            if (!lua)
                return;
            out.count(lua->mScripts.size());
            for (const ESM::LuaScript& script : lua->mScripts)
            {
                out.i32(script.mScriptId);
                out.string(script.mData);
                out.count(script.mTimers.size());
                for (const ESM::LuaTimer& timer : script.mTimers)
                {
                    out.u8(static_cast<std::uint8_t>(timer.mType));
                    out.f64(timer.mTime);
                    out.string(timer.mCallbackName);
                    out.string(timer.mCallbackArgument);
                }
            }
        }

        static constexpr std::size_t luaScriptBytes = 4 + 4 + 4;
        static constexpr std::size_t luaTimerBytes = 1 + 8 + 4 + 4;

        static std::optional<ESM::LuaScripts> readLua(SaveReader& in)
        {
            if (!in.boolean())
                return std::nullopt;
            ESM::LuaScripts lua;
            const std::size_t scripts = in.count(luaScriptBytes);
            for (std::size_t i = 0; i < scripts; ++i)
            {
                ESM::LuaScript script;
                script.mScriptId = in.i32();
                script.mData = in.string();
                const std::size_t timers = in.count(luaTimerBytes);
                for (std::size_t j = 0; j < timers; ++j)
                {
                    ESM::LuaTimer timer;
                    const std::uint8_t type = in.u8();
                    timer.mTime = in.f64();
                    timer.mCallbackName = in.string();
                    timer.mCallbackArgument = in.string();
                    if (type > static_cast<std::uint8_t>(ESM::LuaTimer::Type::REAL_TIME))
                        continue;
                    timer.mType = static_cast<ESM::LuaTimer::Type>(type);
                    script.mTimers.push_back(std::move(timer));
                }
                lua.mScripts.push_back(std::move(script));
            }
            return lua;
        }

        static void writeEntry(SaveWriter& out, const Entry& entry, bool object)
        {
            out.form(entry.mReference);
            if (object)
            {
                out.form(entry.mBase);
                out.u32(entry.mNumber.mIndex);
                out.i32(entry.mNumber.mContentFile);
            }
            out.boolean(entry.mEnabled);
            out.i32(entry.mCount);
            out.f32(entry.mScale);
            writePlace(out, entry.mWhere);
            writeLua(out, entry.mLua);
        }

        /// False when a plugin the entry names is not loaded now (the entry is dropped)
        static bool readEntry(SaveReader& in, Entry& entry, bool object)
        {
            const std::optional<FormId> reference = in.form();
            std::optional<FormId> base = FormId(0);
            if (object)
            {
                base = in.form();
                entry.mNumber.mIndex = in.u32();
                entry.mNumber.mContentFile = in.i32();
            }
            entry.mReference = reference.value_or(0);
            entry.mBase = base.value_or(0);
            entry.mEnabled = in.boolean();
            entry.mCount = in.i32();
            entry.mScale = in.f32();
            const bool placed = readPlace(in, entry.mWhere);
            entry.mLua = readLua(in);
            return reference && base && placed;
        }

        static constexpr std::size_t entryBytes = 4 + 1 + 4 + 4 + 1 + 24 + 1;
        static constexpr std::size_t objectBytes = entryBytes + 4 + 8;
        static constexpr std::size_t moveBytes = 4 + 4 + 1 + 24;
        static constexpr std::size_t requestBytes = 4 + 4 + 4 + 1 + 4 + 1 + 24;

        static void writeReferences(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            std::vector<Entry> changed;
            std::vector<Entry> made;
            OFBase::World* world = OFBase::Environment::get().getWorld();
            OFWorld::WorldModel* model = OFBase::Environment::get().getWorldModel();
            const OFWorld::Ptr player = world != nullptr ? world->getPlayerPtr() : OFWorld::Ptr();
            OFBase::LuaManager* lua = OFBase::Environment::get().getLuaManager();
            if (model != nullptr)
                model->forEachLoadedCellStore([&](OFWorld::CellStore& store) {
                    const ESM::RefId cellId = store.getCell()->getId();
                    store.forEach(
                        [&](const OFWorld::Ptr& ptr) {
                            if (ptr == player || !ESM::isESM4Rec(static_cast<ESM::RecNameInts>(ptr.getType())))
                                return true;
                            const OFWorld::CellRef& ref = ptr.getCellRef();
                            const ESM::Position& position = ptr.getRefData().getPosition();
                            Entry entry;
                            entry.mEnabled = ptr.getRefData().isEnabled();
                            entry.mCount = ref.getCount(false);
                            entry.mScale = ref.getScale();
                            entry.mWhere = { cellId, position.asVec3(), position.asRotationVec3() };
                            // What its Lua scripts keep (the generic save of a cell leaves the references of Fallout
                            // out)
                            if (lua != nullptr && ptr.getRefData().getLuaScripts() != nullptr)
                            {
                                entry.mLua.emplace();
                                lua->saveLocalScripts(ptr, *entry.mLua);
                            }
                            if (ref.hasContentFile())
                            {
                                // What the records say is what a cell that nothing happened to gets again
                                if (!ptr.getRefData().hasChanged() && !ref.hasChanged())
                                    return true;
                                entry.mReference = host.referenceOf(ptr);
                                if (entry.mReference != 0)
                                    changed.push_back(entry);
                                return true;
                            }
                            // An object that was made: one that was deleted is gone
                            if (entry.mCount == 0)
                                return true;
                            entry.mReference = host.referenceOf(ptr);
                            entry.mNumber = ref.getRefNum();
                            const ESM::RefId base = ref.getRefId();
                            if (const ESM::FormId* form = base.getIf<ESM::FormId>();
                                form != nullptr && form->hasContentFile() && form->mContentFile <= 0xfe)
                                entry.mBase = form->toUint32();
                            if (entry.mBase != 0)
                                made.push_back(entry);
                            return true;
                        },
                        true);
                });
            const auto byReference = [](const Entry& a, const Entry& b) { return a.mReference < b.mReference; };
            std::stable_sort(changed.begin(), changed.end(), byReference);
            std::stable_sort(made.begin(), made.end(), byReference);

            // What scripts asked for in cells that were not loaded, and what a loaded game has not handed to a cell yet
            std::map<FormId, Move> relocations = host.mRelocations;
            for (const auto& [reference, move] : host.mPendingMoves)
                relocations[reference].add(move);
            std::map<FormId, bool> disabled = host.mDisabled;
            for (const auto& [reference, state] : host.mPendingToggle)
                disabled[reference] = state;
            {
                // What other threads asked for and the thread of the frame has not queued yet is what scripts see, so
                // it is what the game keeps
                const std::lock_guard lock(host.mDeferredMutex);
                for (const auto& [reference, move] : host.mDeferredPlaces)
                    relocations[reference].add(move);
                for (const auto& [reference, state] : host.mDeferredDisabled)
                    disabled[reference] = state;
            }

            out.count(relocations.size());
            for (const auto& [reference, move] : relocations)
            {
                out.form(reference);
                out.u32(move.mParts);
                writePlace(out, move.mWhere);
            }
            out.count(disabled.size());
            for (const auto& [reference, state] : disabled)
            {
                out.form(reference);
                out.boolean(state);
            }

            out.count(changed.size());
            for (const Entry& entry : changed)
                writeEntry(out, entry, false);

            // A reference that is not in a loaded cell yet keeps what a loaded game said of it
            const OFScript::PendingSave& pending = *host.mPendingSave;
            // and so does a scale that a script gave to one in a cell that was not loaded
            std::map<FormId, OFScript::PendingSave::Reference> unloaded = pending.mReferences;
            {
                const std::lock_guard lock(host.mScalesMutex);
                for (const auto& [reference, scale] : host.mScales)
                    unloaded[reference].mScale = static_cast<float>(scale);
            }
            out.count(unloaded.size());
            for (const auto& [reference, state] : unloaded)
            {
                out.form(reference);
                out.boolean(state.mDeleted);
                out.boolean(state.mScale.has_value());
                out.f32(state.mScale.value_or(1));
                out.boolean(state.mCount.has_value());
                out.i32(state.mCount.value_or(1));
                writeLua(out, state.mLua);
            }

            // The objects that were made, those that are not made yet again included
            for (const OFScript::PendingSave::Object& object : pending.mObjects)
            {
                Entry entry;
                entry.mReference = object.mId;
                entry.mBase = object.mBase;
                entry.mEnabled = object.mEnabled;
                entry.mCount = object.mCount;
                entry.mScale = object.mScale;
                entry.mWhere = { object.mCell, object.mPosition, object.mAngle };
                entry.mNumber = object.mNumber;
                entry.mLua = object.mLua;
                made.push_back(entry);
            }
            out.count(made.size());
            for (const Entry& entry : made)
                writeEntry(out, entry, true);

            // What PlaceAtMe asked for and the world has not made yet is written as it was asked, as where the object
            // goes depends on the reference it is made next to
            std::vector<OFScript::PendingSave::Request> requests = pending.mRequests;
            {
                const std::lock_guard lock(host.mMadeMutex);
                for (const auto& [id, request] : host.mPendingMade)
                {
                    OFScript::PendingSave::Request saved;
                    saved.mId = id;
                    saved.mBase = request.mBase;
                    saved.mNear = request.mNear;
                    saved.mDirection = request.mDirection;
                    saved.mDistance = request.mDistance;
                    saved.mCell = request.mWhere.mCell;
                    saved.mPosition = request.mWhere.mPosition;
                    saved.mAngle = request.mWhere.mAngle;
                    requests.push_back(saved);
                }
            }
            out.count(requests.size());
            for (const OFScript::PendingSave::Request& request : requests)
            {
                out.form(request.mId);
                out.form(request.mBase);
                out.form(request.mNear);
                out.u8(static_cast<std::uint8_t>(request.mDirection));
                out.f32(request.mDistance);
                writePlace(out, { request.mCell, request.mPosition, request.mAngle });
            }
        }

        static void readReferences(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Move> relocations;
            std::map<FormId, bool> disabled;
            std::vector<Entry> changed;
            std::map<FormId, OFScript::PendingSave::Reference> references;
            std::vector<Entry> made;

            const std::size_t moves = in.count(moveBytes);
            for (std::size_t i = 0; i < moves; ++i)
            {
                const std::optional<FormId> reference = in.form();
                Move move;
                move.mParts = in.u32() & Move::All;
                if (readPlace(in, move.mWhere) && reference)
                    relocations[*reference] = move;
            }
            const std::size_t toggles = in.count(5);
            for (std::size_t i = 0; i < toggles; ++i)
            {
                const std::optional<FormId> reference = in.form();
                const bool state = in.boolean();
                if (reference)
                    disabled[*reference] = state;
            }
            const std::size_t changes = in.count(entryBytes);
            for (std::size_t i = 0; i < changes; ++i)
            {
                Entry entry;
                if (readEntry(in, entry, false))
                    changed.push_back(entry);
            }
            const std::size_t pendingReferences = in.count(16);
            for (std::size_t i = 0; i < pendingReferences; ++i)
            {
                const std::optional<FormId> reference = in.form();
                OFScript::PendingSave::Reference state;
                state.mDeleted = in.boolean();
                const bool hasScale = in.boolean();
                const float scale = in.f32();
                const bool hasCount = in.boolean();
                const int count = in.i32();
                state.mLua = readLua(in);
                if (hasScale)
                    state.mScale = scale;
                if (hasCount)
                    state.mCount = count;
                if (reference)
                {
                    references[*reference] = state;
                    // The scripts see the scale the game had given it
                    if (state.mScale)
                    {
                        const std::lock_guard lock(host.mScalesMutex);
                        host.mScales[*reference] = *state.mScale;
                    }
                }
            }
            const std::size_t objects = in.count(objectBytes);
            for (std::size_t i = 0; i < objects; ++i)
            {
                Entry entry;
                if (readEntry(in, entry, true))
                    made.push_back(entry);
            }
            std::vector<OFScript::PendingSave::Request> requests;
            const std::size_t asked = in.count(requestBytes);
            for (std::size_t i = 0; i < asked; ++i)
            {
                const std::optional<FormId> id = in.form();
                const std::optional<FormId> base = in.form();
                const std::optional<FormId> near = in.form();
                OFScript::PendingSave::Request request;
                request.mDirection = in.u8();
                request.mDistance = in.f32();
                Place where;
                const bool placed = readPlace(in, where);
                if (!id || !base || !near || !placed || request.mDirection > 3)
                    continue;
                request.mId = *id;
                request.mBase = *base;
                request.mNear = *near;
                request.mCell = where.mCell;
                request.mPosition = where.mPosition;
                request.mAngle = where.mAngle;
                requests.push_back(request);
            }

            // The references that were seen to change, then what scripts asked of references and the world had not done
            // yet, which comes after (whether the reference is in a loaded cell or not)
            for (const Entry& entry : changed)
            {
                // A reference that was deleted is not put anywhere
                if (entry.mCount == 0)
                {
                    references[entry.mReference].mDeleted = true;
                    continue;
                }
                host.mDisabled[entry.mReference] = !entry.mEnabled;
                Move move;
                move.mWhere = entry.mWhere;
                move.mParts = Move::All;
                host.mRelocations[entry.mReference] = move;
                references[entry.mReference].mScale = entry.mScale;
                // What is left of a stack that was partly taken
                references[entry.mReference].mCount = entry.mCount;
                if (entry.mLua)
                    references[entry.mReference].mLua = entry.mLua;
            }
            for (const auto& [reference, move] : relocations)
            {
                const auto [slot, added] = host.mRelocations.try_emplace(reference, move);
                if (!added)
                    slot->second.add(move);
            }
            for (const auto& [reference, state] : disabled)
                host.mDisabled[reference] = state;
            for (const auto& [reference, state] : references)
                host.mPendingSave->mReferences[reference] = state;
            for (const Entry& entry : made)
            {
                OFScript::PendingSave::Object object;
                object.mId = entry.mReference;
                object.mBase = entry.mBase;
                object.mCell = entry.mWhere.mCell;
                object.mPosition = entry.mWhere.mPosition;
                object.mAngle = entry.mWhere.mAngle;
                object.mEnabled = entry.mEnabled;
                object.mCount = entry.mCount;
                object.mScale = entry.mScale;
                object.mNumber = entry.mNumber;
                object.mLua = entry.mLua;
                host.mPendingSave->mObjects.push_back(object);
            }
            host.mPendingSave->mRequests = std::move(requests);
        }
    };
}

namespace OFScript
{
    ESM::RefId FalloutScripts::refIdOf(FalloutScript::FormId id)
    {
        const ESM::FormId form = ESM::FormId::fromUint32(id);
        return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
    }

    void FalloutScripts::restoreSavedTrigger(FalloutScript::FormId trigger)
    {
        const auto found = mPendingSave->mTriggers.find(trigger);
        if (found == mPendingSave->mTriggers.end())
            return;
        FalloutScript::restoreTrigger(mTriggers, trigger, found->second);
        mPendingSave->mTriggers.erase(found);
    }

    bool FalloutScripts::savedReferenceLoaded(FalloutScript::FormId reference, const OFWorld::Ptr& ptr)
    {
        const auto saved = mPendingSave->mReferences.find(reference);
        if (saved == mPendingSave->mReferences.end())
            return false;
        const PendingSave::Reference state = saved->second;
        mPendingSave->mReferences.erase(saved);
        if (state.mDeleted)
        {
            mPendingSave->mToDelete.push_back(reference);
            return true;
        }
        if (state.mScale)
            mPendingSave->mToScale.emplace_back(reference, *state.mScale);
        if (state.mCount)
            mPendingSave->mToCount.emplace_back(reference, *state.mCount);
        // The scripts of the configuration have been started on it with the cell: those of the save take their place
        if (state.mLua)
            OFBase::Environment::get().getLuaManager()->restoreLocalScripts(ptr, *state.mLua);
        return false;
    }

    std::vector<FalloutScript::SaveSection> FalloutScripts::saveSections()
    {
        using Access = FalloutScript::SaveAccess<FalloutScripts>;
        std::vector<FalloutScript::SaveSection> sections;
        // The first section tells the layout of the whole: the others are only read when it is one this version knows
        sections.push_back({ "format", 1, [this](FalloutScript::SaveWriter& out) { Access::writeFormat(*this, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) { Access::readFormat(*this, in, version); } });
        sections.push_back({ "globals", 1, [this](FalloutScript::SaveWriter& out) { Access::writeGlobals(*this, out); },
            [this](
                FalloutScript::SaveReader& in, std::uint32_t version) { Access::readGlobals(*this, in, version); } });
        sections.push_back({ "quests", FalloutScript::questsSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeQuests(mQuests, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readQuests(mQuests, in, version);
            } });
        sections.push_back({ "objects", FalloutScript::objectScriptsSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeObjectScripts(mObjects, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readObjectScripts(mObjects, in, version);
            } });
        sections.push_back(
            { "controls", 1, [this](FalloutScript::SaveWriter& out) { Access::writeControls(*this, out); },
                [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                    Access::readControls(*this, in, version);
                } });
        sections.push_back({ "talking", 1, [this](FalloutScript::SaveWriter& out) { Access::writeTalking(*this, out); },
            [this](
                FalloutScript::SaveReader& in, std::uint32_t version) { Access::readTalking(*this, in, version); } });
        sections.push_back(
            { "packages", 1, [this](FalloutScript::SaveWriter& out) { Access::writePackages(*this, out); },
                [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                    Access::readPackages(*this, in, version);
                } });
        sections.push_back({ "ai", FalloutScript::aiSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeAi(mAi, out); },
            [this](
                FalloutScript::SaveReader& in, std::uint32_t version) { FalloutScript::readAi(mAi, in, version); } });
        // The order is the order of reading: the values of the actors before the perks that added to them
        const auto section = [&sections](const char* name, std::uint32_t version,
                                 std::function<void(FalloutScript::SaveWriter&)> write,
                                 std::function<void(FalloutScript::SaveReader&, std::uint32_t)> read) {
            sections.push_back({ name, version, std::move(write), std::move(read) });
        };
        section(
            "actors", FalloutScript::actorsSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeActors(mActors, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readActors(mActors, in, version);
            });
        section(
            "equipment", FalloutScript::equipmentSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeEquipment(mEquipment, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readEquipment(mEquipment, in, version);
            });
        section(
            "actorvalues", FalloutScript::actorValuesSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeActorValues(mActorValues, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readActorValues(mActorValues, in, version);
            });
        section(
            "progress", FalloutScript::progressSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeProgress(mProgress, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readProgress(mProgress, in, version);
            });
        section(
            "perks", FalloutScript::perksSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writePerks(mPerks, mActorValues, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readPerks(mPerks, mActorValues, in, version);
            });
        section(
            "itemuse", FalloutScript::itemUseSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeItemUse(mItemUse, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readItemUse(mItemUse, in, version);
            });
        section(
            "combat", FalloutScript::combatSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeCombat(mCombat, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readCombat(mCombat, in, version);
            });
        section(
            "referencestate", FalloutScript::referencesStateSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeReferenceState(mReferenceState, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readReferenceState(mReferenceState, in, version);
            });
        section(
            "reputation", FalloutScript::reputationSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeReputation(mReputation, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readReputation(mReputation, in, version);
            });
        section(
            "objectstate", FalloutScript::objectStateSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeObjectState(mObjectState, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readObjectState(mObjectState, in, version);
            });
        section(
            "magic", FalloutScript::magicSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeMagic(mMagic, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readMagic(mMagic, in, version);
            });
        section(
            "mapmarkers", FalloutScript::mapMarkersSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeMapMarkers(mMapMarkers, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readMapMarkers(mMapMarkers, in, version);
            });
        section(
            "formlists", FalloutScript::formListsSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeFormLists(mFormLists, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readFormLists(mFormLists, in, version);
            });
        section(
            "dialogue", FalloutScript::dialogueSectionVersion,
            [this](FalloutScript::SaveWriter& out) {
                const std::lock_guard lock(mDialogueMutex);
                FalloutScript::writeDialogue(mDialogue, out);
            },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                const std::lock_guard lock(mDialogueMutex);
                FalloutScript::readDialogue(mDialogue, in, version);
            });
        section(
            "triggers", FalloutScript::triggersSectionVersion,
            [this](FalloutScript::SaveWriter& out) {
                FalloutScript::writeTriggers(mTriggers, mPendingSave->mTriggers, out);
            },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                mPendingSave->mTriggers = FalloutScript::readTriggers(in, version);
            });
        section(
            "openstates", FalloutScript::openStatesSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeOpenStates(mOpenStates, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readOpenStates(mOpenStates, in, version);
            });
        section(
            "furniture", 1, [this](FalloutScript::SaveWriter& out) { Access::writeFurniture(*this, out); },
            [this](
                FalloutScript::SaveReader& in, std::uint32_t version) { Access::readFurniture(*this, in, version); });
        section(
            "characters", 1, [this](FalloutScript::SaveWriter& out) { Access::writeCharacters(*this, out); },
            [this](
                FalloutScript::SaveReader& in, std::uint32_t version) { Access::readCharacters(*this, in, version); });
        sections.push_back(
            { "references", 1, [this](FalloutScript::SaveWriter& out) { Access::writeReferences(*this, out); },
                [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                    Access::readReferences(*this, in, version);
                } });
        return sections;
    }

    void FalloutScripts::requestSave(SaveRequest request)
    {
        // The switches of the games, which a request obeys when it is made
        const bool allowed = request == SaveRequest::ForceSave ? Settings::saves().mAllowScriptedForceSave.get()
                                                               : Settings::saves().mAllowScriptedAutosave.get();
        if (allowed)
            mSaveRequests.fetch_or(1u << static_cast<unsigned>(request));
    }

    void FalloutScripts::runSaveRequests()
    {
        const unsigned requested = mSaveRequests.load();
        if (requested == 0)
            return;
        const OFBase::Environment& environment = OFBase::Environment::get();
        OFBase::StateManager* states = environment.getStateManager();
        OFBase::WindowManager* windows = environment.getWindowManager();
        // The request waits while a menu is open, a character is being made, or no game runs
        if (states == nullptr || windows == nullptr || states->getState() != OFBase::StateManager::State_Running
            || environment.getWorld()->getGlobalInt(OFWorld::Globals::sCharGenState) != -1
            || !windows->isSavingAllowed())
            return;
        // One in a frame, in the order of the games
        for (const SaveRequest request : { SaveRequest::Autosave, SaveRequest::ForceSave, SaveRequest::SystemSave })
        {
            const unsigned bit = 1u << static_cast<unsigned>(request);
            if ((requested & bit) == 0)
                continue;
            mSaveRequests.fetch_and(~bit);
            switch (request)
            {
                case SaveRequest::Autosave:
                    states->quickSave("Autosave");
                    break;
                case SaveRequest::ForceSave:
                    // The slot of an earlier one is used again, as for the other two: a game that asks for it often
                    // must not fill the disk
                    states->quickSave("Forced save");
                    break;
                case SaveRequest::SystemSave:
                    states->quickSave("System save");
                    break;
            }
            return;
        }
    }

    FalloutScripts::SavedPlayer FalloutScripts::savedPlayer()
    {
        SavedPlayer result;
        // The player is a character of Fallout when its record is: content of Morrowind that is played with a plugin of
        // Fallout still has the player of Morrowind
        ESM::FormId base;
        if (!baseOf(mPlayer, base) || mStore.get<ESM4::Npc>().search(base) == nullptr)
            return result;
        result.mKnown = true;
        ensureLevellingRules();
        result.mLevel = mProgress.level();
        result.mHealth = mActorValues.current(mPlayer, FalloutScript::ActorValue::health);
        result.mMaxHealth = mActorValues.permanent(mPlayer, FalloutScript::ActorValue::health);
        return result;
    }

    std::size_t FalloutScripts::countSavedGameRecords()
    {
        return saveSections().size();
    }

    void FalloutScripts::write(ESM::ESMWriter& writer, Loading::Listener&)
    {
        for (const FalloutScript::SaveSection& section : saveSections())
        {
            FalloutScript::SaveWriter out;
            section.mWrite(out);
            writer.startRecord(ESM::REC_FOSV);
            writer.writeHNString("NAME", section.mName);
            writer.writeHNT("VERS", section.mVersion);
            // The bytes are not a text: no encoding, and they may hold zeros
            writer.startSubRecord("DATA");
            writer.write(out.bytes().data(), out.bytes().size());
            writer.endRecord("DATA");
            writer.endRecord(ESM::REC_FOSV);
        }
    }

    void FalloutScripts::readRecord(ESM::ESMReader& reader, std::uint32_t type)
    {
        if (type != ESM::REC_FOSV)
        {
            reader.skipRecord();
            return;
        }
        const std::string name = reader.getHNString("NAME");
        std::uint32_t version = 0;
        reader.getHNT(version, "VERS");
        reader.getSubNameIs("DATA");
        reader.getSubHeader();
        std::string data(reader.getSubSize(), '\0');
        reader.getExact(data.data(), data.size());

        const std::vector<FalloutScript::SaveSection> sections = saveSections();
        const auto section = std::find_if(sections.begin(), sections.end(),
            [&name](const FalloutScript::SaveSection& candidate) { return candidate.mName == name; });
        if (section == sections.end())
        {
            Log(Debug::Warning) << "The save has a section of the state of Fallout that this version does not know: "
                                << name;
            return;
        }
        if (version > section->mVersion)
            throw FalloutScript::SaveTooNewError("The section " + name + " of the state of Fallout", version);

        // The plugin of a form of the save may be at another place in the load order now, or not loaded
        FalloutScript::SaveReader in(
            data, [&reader](FalloutScript::FormId saved) -> std::optional<FalloutScript::FormId> {
                ESM::FormId form = ESM::FormId::fromUint32(saved);
                if (!reader.applyContentFileMapping(form))
                    return std::nullopt;
                return form.toUint32();
            });
        try
        {
            section->mRead(in, version);
        }
        catch (const FalloutScript::SaveTooNewError&)
        {
            throw;
        }
        catch (const FalloutScript::SaveError& error)
        {
            throw FalloutScript::SaveError(
                "The section " + name + " of the state of Fallout is damaged: " + error.what());
        }
    }
}
