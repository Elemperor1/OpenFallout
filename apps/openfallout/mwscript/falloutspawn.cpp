#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>

#include <components/debug/debuglog.hpp>
#include <components/esm4/levelled.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadeczn.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadlvlc.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadlvln.hpp>
#include <components/esm4/loadmesg.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnote.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/worldrules.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4base.hpp"
#include "../mwclass/esm4sources.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"
#include "../mwworld/worldmodel.hpp"

namespace OFScript
{
    namespace
    {
        /// The number a script knows a form by; 0 for a form that has none (it is not set, or the world made it up)
        FalloutScript::FormId numberFor(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        /// The cell as the respawn rules know it: the number of its form (0 for a cell that has none)
        FalloutScript::FormId cellKey(const OFWorld::CellStore& cell)
        {
            const ESM::FormId* id = cell.getCell()->getId().getIf<ESM::FormId>();
            return id != nullptr ? numberFor(*id) : 0;
        }

        /// The flag of a container that makes it fill again
        constexpr std::uint8_t containerRespawnFlag = 0x02;
    }

    /// What the commands that bring things into the world, take them out and put them back ask of the game: the world
    /// of the host, and the records of the levelled lists
    class FalloutScripts::SpawnAdapter final : public FalloutScript::SpawnWorld,
                                               public FalloutScript::LevelledListRecords
    {
    public:
        explicit SpawnAdapter(FalloutScripts& host)
            : mHost(host)
        {
        }

        Kind kindOf(FalloutScript::FormId form) override
        {
            const ESM::RefId id = refIdOf(form);
            if (id.empty())
                return Kind::None;
            const OFWorld::ESMStore& store = mHost.mStore;
            if (store.get<ESM4::LevelledItem>().search(id) != nullptr)
                return Kind::Item;
            if (store.get<ESM4::LevelledNpc>().search(id) != nullptr)
                return Kind::Character;
            if (store.get<ESM4::LevelledCreature>().search(id) != nullptr)
                return Kind::Creature;
            return Kind::None;
        }

        bool isEntryOf(Kind kind, FalloutScript::FormId form) override
        {
            const ESM::RefId id = refIdOf(form);
            if (id.empty())
                return false;
            const int type = mHost.mStore.find(id);
            switch (kind)
            {
                case Kind::Item:
                    switch (type)
                    {
                        case ESM::REC_ARMO4:
                        case ESM::REC_WEAP4:
                        case ESM::REC_AMMO4:
                        case ESM::REC_ALCH4:
                        case ESM::REC_MISC4:
                        case ESM::REC_BOOK4:
                        case ESM::REC_INGR4:
                        case ESM::REC_IMOD4:
                        case ESM::REC_KEYM4:
                        case ESM::REC_NOTE4:
                        case ESM::REC_CLOT4:
                            return true;
                        default:
                            return false;
                    }
                case Kind::Character:
                    return type == ESM::REC_NPC_4;
                case Kind::Creature:
                    return type == ESM::REC_CREA4;
                default:
                    return false;
            }
        }

        bool exists(FalloutScript::FormId reference) override
        {
            if (reference == 0)
                return false;
            {
                const std::lock_guard lock(mHost.mMadeMutex);
                if (mHost.mPendingMade.contains(reference))
                    return true;
            }
            // An object that was made while the game runs is known to the world alone, which only the thread of the
            // frame may ask: from another thread it exists until it is known not to
            if ((reference >> 24) == 0xff)
                return std::this_thread::get_id() != mHost.mMainThread || !mHost.findReference(reference).isEmpty();
            const ESM::FormId form = ESM::FormId::fromUint32(reference);
            if (form.isZeroOrUnset())
                return false;
            const OFWorld::ESMStore& store = mHost.mStore;
            return store.get<ESM4::Reference>().search(form) != nullptr
                || store.get<ESM4::ActorCharacter>().search(form) != nullptr
                || store.get<ESM4::ActorCreature>().search(form) != nullptr || store.find(refIdOf(reference)) != 0;
        }

        FalloutScript::FormId placeLeveledActor(
            FalloutScript::FormId near, FalloutScript::FormId base, int levelMod, FalloutScript::FormId zone) override
        {
            using namespace FalloutScript;
            const OFWorld::ESMStore& store = mHost.mStore;
            const ESM::FormId baseForm = ESM::FormId::fromUint32(base);
            const bool isList = store.get<ESM4::LevelledNpc>().search(baseForm) != nullptr
                || store.get<ESM4::LevelledCreature>().search(baseForm) != nullptr;
            if (!isList && store.get<ESM4::Npc>().search(baseForm) == nullptr
                && store.get<ESM4::Creature>().search(baseForm) == nullptr)
            {
                mHost.log("PlaceLeveledActorAtMe: " + hex(base) + " is not a character, a creature or a list of them");
                return 0;
            }

            // The level of the zone the actor comes into (the one that the command names, else that of the cell the
            // reference is in, else that of the worldspace), or the level of the player when there is none
            ESM::FormId zoneForm = ESM::FormId::fromUint32(zone);
            if (zoneForm.isZeroOrUnset())
                zoneForm = zoneOfReference(near);
            const ESM4::EncounterZone* record
                = zoneForm.isZeroOrUnset() ? nullptr : store.get<ESM4::EncounterZone>().search(zoneForm);
            const SpawnSettings& settings = mHost.mSpawnLevels.settings();
            const int playerLevel = mHost.mProgress.level();
            // A zone that has fixed its level keeps it (the cell that came into the game first fixed it: see
            // spawnCellLoaded); one that has not is worked out for the player as he is (Fallout3.exe 0x4EC070, 0x4BFB20)
            int baseLevel = playerLevel;
            if (record != nullptr)
            {
                baseLevel = mHost.mSpawnLevels.lockedZoneLevel(numberFor(zoneForm));
                if (baseLevel == 0)
                    baseLevel = zoneLevel(
                        playerLevel, record->mData.mMinimumLevel, record->mData.mFlags, settings.mLevelScaling);
            }
            const int level = mHost.mSpawnLevels.placementLevel(baseLevel, true);
            const int scaled = scaledSpawnLevel(level, levelMod, settings);
            const int storedMod = storedLevelMod(levelMod);

            // The list gives the character or creature, by dice that differ for each placement
            ESM::FormId chosen = baseForm;
            if (isList)
            {
                FalloutScript::FormId peek = 0;
                {
                    const std::lock_guard lock(mHost.mMadeMutex);
                    peek = mHost.mMadeCount + 1;
                }
                ESM4::LevelledRandom random(0x9E3779B97F4A7C15ull * (static_cast<std::uint64_t>(peek) + 1)
                    + (static_cast<std::uint64_t>(base) << 20) + static_cast<std::uint64_t>(scaled));
                chosen = chooseActor(baseForm, scaled, storedMod, random);
                if (chosen.isZeroOrUnset())
                {
                    mHost.log("PlaceLeveledActorAtMe: the list " + hex(base) + " gives nothing at level "
                        + std::to_string(scaled));
                    return 0;
                }
            }
            const FormId made = mHost.placeAtMe(near, numberFor(chosen), 1, 0.0, 0);
            if (made == 0)
                return 0;
            mHost.mSpawnLevels.set(made, scaled, levelMod);
            mHost.log("PlaceLeveledActorAtMe: " + hex(base) + " placed as " + hex(made) + " from " + hex(numberFor(chosen))
                + " at level " + std::to_string(scaled) + " (zone level " + std::to_string(baseLevel) + ", placed at "
                + std::to_string(level) + ", modifier " + std::to_string(levelMod) + ", player "
                + std::to_string(playerLevel) + ")");
            return made;
        }

        FalloutScript::FormId holderOfItem(FalloutScript::FormId item) override
        {
            const auto found = mHost.mItemHolders.find(item);
            return found != mHost.mItemHolders.end() ? found->second : 0;
        }

        FalloutScript::FormId dropItem(FalloutScript::FormId holder, FalloutScript::FormId item, int count, int condition,
            int mods) override
        {
            if (holder == 0 || item == 0 || count <= 0)
                return 0;
            const FalloutScript::FormId made = mHost.placeAtMe(holder, item, 1, 0.0, 0);
            if (made == 0)
                return 0;
            // The object keeps how worn the copies were and the mods in them, to be picked up with them
            mHost.mReferenceState.setCondition(made, condition, mods);
            // The stack is made as one object at the start of the next frame
            const std::lock_guard lock(mHost.mMadeMutex);
            if (const auto asked = mHost.mPendingMade.find(made); asked != mHost.mPendingMade.end())
                asked->second.mCount = count;
            return made;
        }

        int formTypeOf(FalloutScript::FormId form) override
        {
            // The number of the record's signature in the executable's table; the same mapping as the form type
            // conditions, so every inventory record (keys, notes, weapon mods and clothes too) has its number
            const int type = mHost.mStore.find(refIdOf(form));
            if (type == 0)
                return 0;
            const int number = FalloutScript::World::formTypeNumber(
                FalloutScript::World::signatureOfRecordType(static_cast<std::uint32_t>(type)));
            return number > 0 ? number : 0;
        }

        FalloutScript::FormId parentOf(FalloutScript::FormId reference) override
        {
            const ESM::FormId form = ESM::FormId::fromUint32(reference);
            if (form.isZeroOrUnset())
                return 0;
            const OFWorld::ESMStore& store = mHost.mStore;
            if (const ESM4::Reference* record = store.get<ESM4::Reference>().search(form))
                return numberFor(record->mEsp.parent);
            if (const ESM4::ActorCharacter* record = store.get<ESM4::ActorCharacter>().search(form))
                return numberFor(record->mEsp.parent);
            if (const ESM4::ActorCreature* record = store.get<ESM4::ActorCreature>().search(form))
                return numberFor(record->mEsp.parent);
            return 0;
        }

        void purgeCellBuffers() override
        {
            // The games drop the cells they keep in memory for a quick return. The cells here are kept by the world as
            // long as they are near, and loaded again from the files when they are not: there is nothing to drop.
            mHost.log("PurgeCellBuffers: no buffer of cells is kept");
        }

    private:
        static ESM::RefId refIdOf(FalloutScript::FormId id) { return FalloutScripts::refIdOf(id); }

        /// The zone of the cell the reference is in, else that of the worldspace of the cell; unset for none
        ESM::FormId zoneOfReference(FalloutScript::FormId reference)
        {
            Place place;
            if (!mHost.whereIs(reference, place))
                return ESM::FormId();
            const ESM::FormId* id = place.mCell.getIf<ESM::FormId>();
            const ESM4::Cell* cell = id != nullptr ? mHost.mStore.get<ESM4::Cell>().search(*id) : nullptr;
            if (cell == nullptr)
                return ESM::FormId();
            if (!cell->mEncounterZone.isZeroOrUnset())
                return cell->mEncounterZone;
            const ESM::FormId* worldId = cell->mParent.getIf<ESM::FormId>();
            const ESM4::World* world = worldId != nullptr ? mHost.mStore.get<ESM4::World>().search(*worldId) : nullptr;
            return world != nullptr ? world->mEncounterZone : ESM::FormId();
        }

        /// The character or creature that the list gives for the level; unset when it gives none
        ESM::FormId chooseActor(const ESM::FormId& list, int level, int levelMod, ESM4::LevelledRandom& random)
        {
            const OFWorld::ESMStore& store = mHost.mStore;
            if (store.get<ESM4::LevelledCreature>().search(list) != nullptr)
            {
                const ESM4::Creature* creature = ESM4::resolveLevelledRecord<ESM4::Creature>(
                    [&store](ESM::FormId id) { return store.get<ESM4::Creature>().search(id); },
                    [&store](ESM::FormId id) { return store.get<ESM4::LevelledCreature>().search(id); }, list, level,
                    random, nullptr, 0, levelMod);
                return creature != nullptr ? creature->mId : ESM::FormId();
            }
            const ESM4::Npc* npc = ESM4::resolveLevelledRecord<ESM4::Npc>(
                [&store](ESM::FormId id) { return store.get<ESM4::Npc>().search(id); },
                [&store](ESM::FormId id) { return store.get<ESM4::LevelledNpc>().search(id); }, list, level, random,
                nullptr, 0, levelMod);
            return npc != nullptr ? npc->mId : ESM::FormId();
        }

        FalloutScripts& mHost;
    };

    void FalloutScripts::setupSpawn()
    {
        mSpawnAdapter = std::make_shared<SpawnAdapter>(*this);
        FalloutScript::addLevelledListCommands(mInterpreter, mLevelledLists, *mSpawnAdapter);
        const FalloutScript::SpawnParts parts{ mActors, mFormLists, *this, *mSpawnAdapter, mRespawn, mItemValues,
            mNameOverrides, mEquipment };
        FalloutScript::addSpawnCommands(mInterpreter, parts);

        // What scripts added to a list is part of the list for every roll of it, wherever it is rolled
        ESM4::setLevelledAdditions(
            [this](ESM::FormId list, const std::vector<ESM4::LVLO>& entries, std::vector<ESM4::LVLO>& merged) {
                const FalloutScript::FormId number = numberFor(list);
                return number != 0 && mLevelledLists.merge(number, entries, merged);
            });
        // The level an actor that scripts placed is rolled for: the world's own number of it, or the one scripts know
        // it by. Every other actor is rolled for the level that the cells use.
        ESM4::setSpawnRollProvider([this](ESM::FormId reference) {
            ESM4::SpawnRoll roll{ OFClass::ESM4Impl::sDefaultLevel, 0 };
            if (reference.isZeroOrUnset())
                return roll;
            {
                const std::lock_guard lock(mWorldRollMutex);
                if (const auto found = mWorldRolls.find(reference); found != mWorldRolls.end())
                {
                    roll.mLevel = found->second.first;
                    roll.mLevelMod = FalloutScript::storedLevelMod(found->second.second);
                    return roll;
                }
            }
            // A number that scripts made up (0xFF and a counter) is the key of the table of the placements
            if (reference.mContentFile == 0xff)
            {
                int level = 0;
                int levelMod = 0;
                if (mSpawnLevels.find(0xff000000u | (reference.mIndex & 0xffffffu), level, levelMod))
                {
                    roll.mLevel = level;
                    roll.mLevelMod = FalloutScript::storedLevelMod(levelMod);
                }
                return roll;
            }
            // An actor of a cell is rolled for the level that was chosen when it was first made
            placedActorRoll(reference, roll);
            return roll;
        });
    }

    bool FalloutScripts::placedActorRoll(const ESM::FormId& reference, ESM4::SpawnRoll& roll)
    {
        using namespace FalloutScript;
        const FormId key = numberFor(reference);
        if (key == 0)
            return false;
        const ESM4::ActorCharacter* placed = mStore.get<ESM4::ActorCharacter>().search(reference);
        if (placed == nullptr)
            placed = mStore.get<ESM4::ActorCreature>().search(reference);
        if (placed == nullptr)
            return false;

        // Whether the template of its base is a levelled list of characters or creatures, which the executable resolves
        // when the actor is made (0x55D760 through 0x47CDD0 and 0x47CE10)
        bool leveledTemplate = false;
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(placed->mBaseObj))
            leveledTemplate = npc->mIsFONV && !npc->mBaseTemplate.isZeroOrUnset()
                && mStore.get<ESM4::LevelledNpc>().search(npc->mBaseTemplate) != nullptr;
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(placed->mBaseObj))
            leveledTemplate = creature->mIsFONV && !creature->mBaseTemplate.isZeroOrUnset()
                && mStore.get<ESM4::LevelledCreature>().search(creature->mBaseTemplate) != nullptr;
        else
            return false;

        // The zone of the reference, else of its cell, else of the worldspace (0x567D20)
        ESM::FormId cellZone;
        ESM::FormId worldZone;
        if (const ESM::FormId* cellId = placed->mParent.getIf<ESM::FormId>())
        {
            if (const ESM4::Cell* cell = mStore.get<ESM4::Cell>().search(*cellId))
            {
                cellZone = cell->mEncounterZone;
                const ESM::FormId* worldId = cell->mParent.getIf<ESM::FormId>();
                if (const ESM4::World* world = worldId != nullptr ? mStore.get<ESM4::World>().search(*worldId) : nullptr)
                    worldZone = world->mEncounterZone;
            }
        }
        const FormId zone
            = placementZone(numberFor(placed->mEncounterZone), numberFor(cellZone), numberFor(worldZone));
        const ESM4::EncounterZone* zoneRecord
            = zone == 0 ? nullptr : mStore.get<ESM4::EncounterZone>().search(ESM::FormId::fromUint32(zone));
        const SpawnSettings& settings = mSpawnLevels.settings();
        const int playerLevel = mProgress.level();
        // A zone that has fixed its level keeps it (the cell that came into the game first fixed it, see spawnCellLoaded);
        // one that has not is worked out for the player as he is
        int baseLevel = playerLevel;
        if (zoneRecord != nullptr)
        {
            baseLevel = mSpawnLevels.lockedZoneLevel(zone);
            if (baseLevel == 0)
                baseLevel = zoneLevel(
                    playerLevel, zoneRecord->mData.mMinimumLevel, zoneRecord->mData.mFlags, settings.mLevelScaling);
        }

        int level = 0;
        int levelMod = 0;
        // (only an actor whose template is a levelled list is rolled for the level of its zone: any other is for the level of
        // the player as it is)
        const bool made = mSpawnLevels.placedLevel(
            key, leveledTemplate ? baseLevel : playerLevel, placed->mLevelModifier, leveledTemplate, level, levelMod);
        if (made && leveledTemplate)
            Log(Debug::Verbose) << "Placed actor " << hex(key) << " rolled for level " << level << " (zone level "
                                << baseLevel << ", modifier " << placed->mLevelModifier << ", player " << playerLevel
                                << ")";
        roll.mLevel = level;
        roll.mLevelMod = storedLevelMod(levelMod);
        return true;
    }

    std::vector<FalloutScript::FormId> FalloutScripts::templateChainOf(FalloutScript::FormId actor)
    {
        std::vector<FalloutScript::FormId> chain;
        ESM::FormId base;
        if (actor == 0 || !baseOf(actor, base) || base.isZeroOrUnset())
            return chain;
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
        const ESM4::SpawnRoll roll
            = ESM4::spawnRollOf(ESM::FormId::fromUint32(actor), OFClass::ESM4Impl::sDefaultLevel);
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            const OFClass::StoreWornArmorSource source(mStore);
            for (const ESM4::Npc* record : ESM4::templateChain(source, *npc, roll.mLevel, seed, nullptr, roll.mLevelMod))
                chain.push_back(numberFor(record->mId));
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const OFClass::StoreCreatureSource source(mStore);
            for (const ESM4::Creature* record
                : ESM4::creatureTemplateChain(source, *creature, roll.mLevel, seed, roll.mLevelMod))
                chain.push_back(numberFor(record->mId));
        }
        return chain;
    }

    void FalloutScripts::clearSpawn()
    {
        mLevelledLists.clear();
        mSpawnLevels.clear();
        mRespawn.clear();
        mItemValues.clear();
        mNameOverrides.clear();
        // A game that was loaded looks at the merchants again, whatever day the one before it had last looked on
        mMerchantCheckedDay = -1;
        mMerchantScanDue = false;
        const std::lock_guard lock(mWorldRollMutex);
        mWorldRolls.clear();
    }

    bool FalloutScripts::respawnsByRecord(FalloutScript::FormId actor)
    {
        ESM::FormId base;
        if (actor == 0 || !baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
            return respawnsAsRecord(*npc, actor);
        if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
            return respawnsAsRecord(*creature, actor);
        return false;
    }

    void FalloutScripts::updateSpawn()
    {
        if (mQuickSaveRequested.exchange(false))
            OFBase::Environment::get().getStateManager()->quickSave();
        const auto today = static_cast<std::int64_t>(std::floor(gameDaysPassed()));
        // A new day, or a cell that was loaded since: the merchants that came in after the first look of the day are
        // looked at too (each is restocked once a day, whenever it comes)
        if (!mMerchantScanDue.exchange(false) && today == mMerchantCheckedDay)
            return;
        mMerchantCheckedDay = today;
        // On the days the settings name, the merchants that are up (loaded characters and creatures with a container
        // of merchandise) get new stock: the inventory of the container is rolled again (0x886360 asks the container
        // for the same thing ResetInventory does)
        int restocked = 0;
        for (const auto& [reference, ptr] : mReferences)
        {
            if (ptr.isEmpty() || reference == mPlayer || leftLoadedCells(reference))
                continue;
            if (ptr.getType() != ESM4::Npc::sRecordId && ptr.getType() != ESM4::Creature::sRecordId)
                continue;
            const FalloutScript::FormId container = merchantContainer(reference);
            if (container == 0 || !mRespawn.merchantRestocks(reference, today))
                continue;
            mRespawn.resetInventory(container);
            mActors.resetItems(container);
            ++restocked;
        }
        if (restocked > 0)
            Log(Debug::Info) << "Respawn: day " << today << " restocks " << restocked << " merchants";
    }

    std::string_view FalloutScripts::nameGivenToBase(const ESM::FormId& base)
    {
        FalloutScript::FormId message = 0;
        const FalloutScript::FormId key = numberFor(base);
        if (key == 0 || !mNameOverrides.find(key, message))
            return {};
        const std::lock_guard lock(mNameTextMutex);
        if (const auto found = mNameTextOf.find(key); found != mNameTextOf.end() && found->second.first == message)
            return mNameTexts[found->second.second];
        // The full name of the form that was given: a message has its title, the other kinds their own full name
        const ESM::FormId form = ESM::FormId::fromUint32(message);
        std::string text;
        if (const ESM4::Message* record = mStore.get<ESM4::Message>().search(form))
            text = record->mFullName;
        else if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(form))
            text = misc->mFullName;
        else if (const ESM4::Note* note = mStore.get<ESM4::Note>().search(form))
            text = note->mFullName;
        else if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(form))
            text = npc->mFullName;
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(form))
            text = creature->mFullName;
        if (text.empty())
            return {};
        mNameTexts.push_back(std::move(text));
        mNameTextOf[key] = { message, mNameTexts.size() - 1 };
        return mNameTexts.back();
    }

    bool FalloutScripts::zoneNeverResets(FalloutScript::FormId zone)
    {
        if (zone == 0)
            return false;
        const ESM4::EncounterZone* record = mStore.get<ESM4::EncounterZone>().search(ESM::FormId::fromUint32(zone));
        return mRespawn.zoneNeverResets(zone, record != nullptr ? record->mData.mFlags : 0);
    }

    bool FalloutScripts::respawnsContainerByRecord(FalloutScript::FormId container)
    {
        ESM::FormId base;
        if (container == 0 || !baseOf(container, base) || base.isZeroOrUnset())
            return false;
        const ESM4::Container* record = mStore.get<ESM4::Container>().search(base);
        return record != nullptr && (record->mDataFlags & containerRespawnFlag) != 0;
    }

    std::uint32_t FalloutScripts::inventoryGeneration(FalloutScript::FormId holder) const
    {
        return static_cast<std::uint32_t>(mRespawn.inventoryGeneration(holder));
    }

    ESM::FormId FalloutScripts::nextGeneratedNumber() const
    {
        const OFWorld::WorldModel* model = OFBase::Environment::get().getWorldModel();
        if (model == nullptr)
            return ESM::FormId();
        ESM::FormId next = model->getLastGeneratedRefNum();
        // The counter of the world goes up before it gives a number (CellRef::getOrAssignRefNum)
        ++next.mIndex;
        return next;
    }

    void FalloutScripts::spawnRollFor(FalloutScript::FormId id, const ESM::FormId& number)
    {
        int level = 0;
        int levelMod = 0;
        if (number.isZeroOrUnset() || !mSpawnLevels.find(id, level, levelMod))
            return;
        const std::lock_guard lock(mWorldRollMutex);
        mWorldRolls[number] = { level, levelMod };
    }

    void FalloutScripts::spawnPlaced(FalloutScript::FormId id, const OFWorld::Ptr& placed, const ESM::FormId& guessed)
    {
        int level = 0;
        int levelMod = 0;
        if (!mSpawnLevels.find(id, level, levelMod))
            return;
        const ESM::FormId actual = placed.getCellRef().getRefNum();
        if (actual == guessed)
            return;
        // The world gave the object another number than the next one it had: the roll goes with the right one (what was
        // chosen for the object while it was put in the cell used the default level)
        log("PlaceLeveledActorAtMe: the object " + FalloutScript::hex(id) + " has not the number that was guessed");
        const std::lock_guard lock(mWorldRollMutex);
        mWorldRolls.erase(guessed);
        mWorldRolls[actual] = { level, levelMod };
    }

    void FalloutScripts::spawnActorDied(FalloutScript::FormId actor)
    {
        mRespawn.died(actor, FalloutScript::gameHoursOf(gameDaysPassed()));
    }

    void FalloutScripts::spawnCellUnloaded(OFWorld::CellStore& cell)
    {
        if (const FalloutScript::FormId key = cellKey(cell); key != 0)
            mRespawn.cellDetached(key, FalloutScript::gameHoursOf(gameDaysPassed()));
    }

    void FalloutScripts::spawnCellLoaded(OFWorld::CellStore& cell)
    {
        mMerchantScanDue = true;
        const FalloutScript::FormId key = cellKey(cell);
        if (key == 0)
            return;
        const std::uint32_t now = FalloutScript::gameHoursOf(gameDaysPassed());

        // Whether the zone of the cell says that it never resets: what a script told it, or the flag of its record
        const ESM4::Cell* record = mStore.get<ESM4::Cell>().search(ESM::FormId::fromUint32(key));
        ESM::FormId zone;
        if (record != nullptr)
        {
            zone = record->mEncounterZone;
            if (zone.isZeroOrUnset())
            {
                const ESM::FormId* worldId = record->mParent.getIf<ESM::FormId>();
                const ESM4::World* world = worldId != nullptr ? mStore.get<ESM4::World>().search(*worldId) : nullptr;
                if (world != nullptr)
                    zone = world->mEncounterZone;
            }
        }
        bool neverResets = false;
        if (!zone.isZeroOrUnset())
        {
            const ESM4::EncounterZone* zoneRecord = mStore.get<ESM4::EncounterZone>().search(zone);
            // The zone of a cell that comes into the game fixes its level, once (Fallout3.exe 0x4BFBA0 called at 0x43A141,
            // New Vegas 0x526270 called at 0x453F07): for the player of that moment
            if (zoneRecord != nullptr)
                mSpawnLevels.lockZoneLevel(numberFor(zone),
                    FalloutScript::zoneLevel(mProgress.level(), zoneRecord->mData.mMinimumLevel,
                        zoneRecord->mData.mFlags, mSpawnLevels.settings().mLevelScaling));
            neverResets = zoneNeverResets(numberFor(zone));
        }
        if (!mRespawn.attachCell(key, now, neverResets))
            return;

        // The cell was left for long enough: the characters that come back are alive with their health again and a
        // new inventory, and the containers that fill again are filled
        int revived = 0;
        int refilled = 0;
        int rerolled = 0;
        cell.forEach([&](const OFWorld::Ptr& ptr) {
            const FalloutScript::FormId reference = referenceOf(ptr);
            if (reference == 0)
                return true;
            if (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId)
            {
                const bool dead = mActors.isDead(reference);
                const bool due = mRespawn.actorDue(reference, now, neverResets);
                if ((reference >> 24) != 0xff && (!dead || due))
                {
                    // The cell reset: an actor that is up to be made new goes back to its own base and resolves its
                    // template again, for the level of the player now (0x550C60 through 0x47CE10)
                    mSpawnLevels.forget(reference);
                    // (what was read from the old template is read again from the new one)
                    mFightTraits.erase(reference);
                    ptr.getRefData().setCustomData(nullptr);
                    ++rerolled;
                }
                if (!dead || !due)
                    return true;
                // The flag of the record that has the base data (a template can give it)
                const bool respawns = ptr.getType() == ESM4::Npc::sRecordId
                    ? respawnsAsRecord(*ptr.get<ESM4::Npc>()->mBase, reference)
                    : respawnsAsRecord(*ptr.get<ESM4::Creature>()->mBase, reference);
                if (!respawns)
                    return true;
                {
                    // Alive again and with its health back in one step, as ResurrectActor does it
                    const auto transitions = mActors.holdTransitions();
                    mActors.revive(reference);
                    mActorValues.heal(reference, FalloutScript::ActorValue::health);
                }
                mRespawn.revived(reference);
                mRespawn.resetInventory(reference);
                mActors.resetItems(reference);
                // It is dressed from its record again, as what a looted corpse lost is carried again
                mEquipment.reset(reference);
                ++revived;
            }
            else if (ptr.getType() == ESM4::Container::sRecordId)
            {
                if ((ptr.get<ESM4::Container>()->mBase->mDataFlags & containerRespawnFlag) == 0)
                    return true;
                mRespawn.resetInventory(reference);
                mActors.resetItems(reference);
                ++refilled;
            }
            return true;
        });
        // A cell that has nothing to bring back is not worth a line
        if (revived > 0 || refilled > 0 || rerolled > 0)
            log("Respawn: the cell " + FalloutScript::hex(key) + " reset at game hour " + std::to_string(now) + ": "
                + std::to_string(revived) + " characters came back, " + std::to_string(refilled)
                + " containers were filled again, " + std::to_string(rerolled) + " actors resolve their template again");
    }
}
