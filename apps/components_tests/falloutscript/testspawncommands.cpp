#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/equipment.hpp>
#include <components/falloutscript/formlists.hpp>
#include <components/falloutscript/objectcommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/respawn.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/spawncommands.hpp>
#include <components/falloutscript/spawnrules.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId crate = 0x01000101;
    constexpr FormId crateBase = 0x01000201;
    constexpr FormId cellA = 0x01000A00;
    constexpr FormId cellB = 0x01000A01;
    constexpr FormId gangerBase = 0x01000500;
    constexpr FormId zone = 0x01000C00;
    constexpr FormId pistol = 0x01000400;
    constexpr FormId rifle = 0x01000401;
    constexpr FormId junk = 0x01000402;
    constexpr FormId vest = 0x01000403;
    constexpr FormId junkList = 0x01000700;
    constexpr FormId message = 0x01000800;
    constexpr FormId scriptOfGuards = 0x01002000;
    constexpr FormId scriptOfItems = 0x01002001;
    constexpr FormId made = 0x01000999;

    constexpr std::uint16_t OpDrop = 0x1057;
    constexpr std::uint16_t OpRemoveMe = 0x10A5;
    constexpr std::uint16_t OpDropMe = 0x10A6;
    constexpr std::uint16_t OpGetParentRef = 0x1113;
    constexpr std::uint16_t OpSetCellFullName = 0x111B;
    constexpr std::uint16_t OpSetActorFullName = 0x111C;
    constexpr std::uint16_t OpResetInterior = 0x1127;
    constexpr std::uint16_t OpSetItemValue = 0x1155;
    constexpr std::uint16_t OpExists = 0x119F;
    constexpr std::uint16_t OpPurgeCellBuffers = 0x1189;
    constexpr std::uint16_t OpGetInCellParam = 0x10E6;
    constexpr std::uint16_t OpPlaceAtMeHealthPercent = 0x11BD;
    constexpr std::uint16_t OpPlaceLeveledActorAtMe = 0x11D1;
    constexpr std::uint16_t OpSetZoneRespawns = 0x11EA;
    constexpr std::uint16_t OpResetInventory = 0x11ED;
    constexpr std::uint16_t OpRemoveAllTypedItems = 0x1249;
    constexpr std::uint16_t OpGetContainerInventoryCount = 0x127A;

    constexpr std::uint32_t integerVariable = 1;

    class FakeActorRecords : public FalloutScript::ActorRecords, public FalloutScript::ItemRecords
    {
    public:
        std::map<FormId, std::vector<std::pair<FormId, int>>> mItems;
        std::map<FormId, std::vector<FormId>> mStartingArmor;
        int mQuestions = 0;

        bool itemInfo(FormId item, FalloutScript::ItemInfo& info) override
        {
            // Every item of the tests that is worn is a vest
            if (item != vest)
                return false;
            info.mKind = FalloutScript::ItemKind::Armor;
            info.mSlots = FalloutScript::BipedSlot::upperBody;
            return true;
        }

        bool startingArmor(FormId actor, std::vector<FormId>& armor) override
        {
            armor = mStartingArmor[actor];
            return true;
        }

        bool initialItems(FormId holder, std::vector<std::pair<FormId, int>>& items) override
        {
            ++mQuestions;
            const auto found = mItems.find(holder);
            if (found != mItems.end())
                items = found->second;
            return true;
        }

        bool initialFactions(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
    };

    class FakeObjects : public FalloutScript::ObjectWorld
    {
    public:
        struct Placed
        {
            FormId mNear, mBase;
            int mCount;
            double mDistance;
            int mDirection;
        };
        std::vector<Placed> mPlaced;
        std::map<FormId, FormId> mCells;
        std::map<FormId, FormId> mBases;

        bool isDisabled(FormId) override { return false; }
        void setDisabled(FormId, bool) override {}
        bool distance(FormId, FormId, double&) override { return false; }
        FormId cellOf(FormId reference) override
        {
            const auto found = mCells.find(reference);
            return found == mCells.end() ? 0 : found->second;
        }
        void activate(FormId, FormId, bool) override {}
        bool baseFormOf(FormId reference, FormId& base) override
        {
            const auto found = mBases.find(reference);
            if (found == mBases.end())
                return false;
            base = found->second;
            return true;
        }
        FormId placeAtMe(FormId reference, FormId base, int count, double distance, int direction) override
        {
            mPlaced.push_back({ reference, base, count, distance, direction });
            return made;
        }
    };

    class FakeWorld : public FalloutScript::SpawnWorld
    {
    public:
        struct Leveled
        {
            FormId mNear, mBase, mZone;
            int mLevelMod;
        };
        struct Dropped
        {
            FormId mHolder, mItem;
            int mCount;
            int mCondition;
            int mMods;
        };
        std::set<FormId> mExisting;
        std::vector<Leveled> mLeveled;
        std::vector<Dropped> mDropped;
        std::map<FormId, FormId> mHolders;
        std::map<FormId, int> mTypes;
        std::map<FormId, FormId> mParents;
        int mPurged = 0;
        /// The world takes no drop (nothing can be placed)
        bool mRefuseDrops = false;

        bool exists(FormId reference) override { return mExisting.count(reference) != 0; }
        FormId placeLeveledActor(FormId near, FormId base, int levelMod, FormId zoneForm) override
        {
            mLeveled.push_back({ near, base, zoneForm, levelMod });
            return made;
        }
        FormId holderOfItem(FormId item) override
        {
            const auto found = mHolders.find(item);
            return found == mHolders.end() ? 0 : found->second;
        }
        FormId dropItem(FormId holder, FormId item, int count, int condition, int mods) override
        {
            if (mRefuseDrops)
                return 0;
            mDropped.push_back({ holder, item, count, condition, mods });
            return made;
        }
        int formTypeOf(FormId form) override
        {
            const auto found = mTypes.find(form);
            return found == mTypes.end() ? 0 : found->second;
        }
        FormId parentOf(FormId reference) override
        {
            const auto found = mParents.find(reference);
            return found == mParents.end() ? 0 : found->second;
        }
        void purgeCellBuffers() override { ++mPurged; }
    };

    class FakeFormListRecords : public FalloutScript::FormListRecords
    {
    public:
        std::map<FormId, std::vector<FormId>> mLists;
        bool formListEntries(FormId list, std::vector<FormId>& entries) override
        {
            const auto found = mLists.find(list);
            if (found == mLists.end())
                return false;
            entries = found->second;
            return true;
        }
        bool baseFormOf(FormId, FormId&) override { return false; }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        FakeActorRecords mActorRecords;
        FalloutScript::ActorState mActors{ mActorRecords };
        FakeFormListRecords mFormListRecords;
        FalloutScript::FormLists mFormLists{ mFormListRecords };
        FakeObjects mObjectWorld;
        FakeWorld mWorld;
        FalloutScript::RespawnState mRespawn;
        FalloutScript::ItemValues mItemValues;
        FalloutScript::NameOverrides mNames;
        FalloutScript::Equipment mEquipment{ mActorRecords, mActors };

        Runner()
        {
            FalloutScript::addSpawnCommands(mInterpreter,
                { mActors, mFormLists, mObjectWorld, mWorld, mRespawn, mItemValues, mNames, mEquipment });
        }

        void run(const ScriptBuilder& builder, FormId owner = guard)
        {
            ESM4::Script& record = mScripts[scriptOfGuards + (owner == guard ? 0 : 1)];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards + (owner == guard ? 0 : 1));
            record.mScript = builder.definition();
            mObjects.add(owner, record.mId.toUint32());
            mObjects.trigger(owner, BlockType::OnActivate);
        }

        double variable(std::uint32_t index, FormId owner = guard)
        {
            FalloutScript::Instance* instance = mObjects.instance(owner);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    ScriptBuilder oneCommand(std::uint16_t opcode, const Bytes& args, std::uint16_t reference = 0)
    {
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(opcode, args, reference) + end());
        return builder;
    }

    TEST(FalloutScriptSpawnCommandsTest, aLeveledActorIsMadeAtTheReferenceWithTheModifierAndTheZone)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(gangerBase);
        const std::uint16_t zoneIndex = builder.form(zone);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpPlaceLeveledActorAtMe, arguments(3).add(refArg(base)).add(intArg(2)).add(refArg(zoneIndex))))
            + command(OpPlaceLeveledActorAtMe, arguments(1).add(refArg(base))) + end());
        builder.variable(1, integerVariable, "made");

        setup.run(builder);

        ASSERT_EQ(setup.mWorld.mLeveled.size(), 2u);
        EXPECT_EQ(setup.mWorld.mLeveled[0].mNear, guard);
        EXPECT_EQ(setup.mWorld.mLeveled[0].mBase, gangerBase);
        EXPECT_EQ(setup.mWorld.mLeveled[0].mLevelMod, 2);
        EXPECT_EQ(setup.mWorld.mLeveled[0].mZone, zone);
        // without a modifier it is 4, which changes nothing
        EXPECT_EQ(setup.mWorld.mLeveled[1].mLevelMod, FalloutScript::noLevelMod);
        EXPECT_EQ(setup.mWorld.mLeveled[1].mZone, 0u);
        EXPECT_EQ(setup.variable(1), static_cast<double>(made));
    }

    TEST(FalloutScriptSpawnCommandsTest, aReferenceExistsWhenItIsKnownAndIsNotTheOneNamed)
    {
        Runner setup;
        setup.mWorld.mExisting = { guard };
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(crate);
        const std::uint16_t self = builder.form(guard);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpExists, arguments(1).add(refArg(other))))
            + setTo(var('s', 2), callToken(OpExists, arguments(1).add(refArg(self))))
            + setTo(var('s', 3), callToken(OpExists, arguments(1).add(refArg(self)), other))
            + end());
        builder.variable(1, integerVariable, "known").variable(2, integerVariable, "itself").variable(3, integerVariable, "unknown");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
        EXPECT_EQ(setup.variable(3), 0); // the crate is not known to the host
    }

    TEST(FalloutScriptSpawnCommandsTest, theItemRemovesItselfFromTheHolderAndGoesWhereItIsTold)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 3 } };
        setup.mActorRecords.mItems[crate] = {};
        setup.mWorld.mHolders[pistol] = guard;
        ScriptBuilder builder;
        const std::uint16_t destination = builder.form(crate);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpRemoveMe, arguments(1).add(refArg(destination)))
            + command(OpRemoveMe, arguments(0)) + end());

        setup.run(builder, pistol);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 1);
        EXPECT_EQ(setup.mActors.itemCount(crate, pistol), 1);
        EXPECT_THAT(setup.mWorld.mDropped, IsEmpty());
    }

    TEST(FalloutScriptSpawnCommandsTest, anItemThatNoHolderHasChangesNothing)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 3 } };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpRemoveMe, arguments(0))
            + command(OpDropMe, arguments(0)) + end());

        setup.run(builder, pistol);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 3);
        EXPECT_THAT(setup.mWorld.mDropped, IsEmpty());
    }

    TEST(FalloutScriptSpawnCommandsTest, theItemDropsItselfIntoTheWorld)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 } };
        setup.mWorld.mHolders[pistol] = guard;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpDropMe, arguments(0)) + end());

        setup.run(builder, pistol);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 1);
        ASSERT_EQ(setup.mWorld.mDropped.size(), 1u);
        EXPECT_EQ(setup.mWorld.mDropped[0].mHolder, guard);
        EXPECT_EQ(setup.mWorld.mDropped[0].mItem, pistol);
        EXPECT_EQ(setup.mWorld.mDropped[0].mCount, 1);
    }

    TEST(FalloutScriptSpawnCommandsTest, aReferenceDropsAsManyOfAnItemAsItHas)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 4 } };
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(pistol);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpDrop, arguments(2).add(refArg(item)).add(intArg(10)))
            + command(OpDrop, arguments(2).add(refArg(item)).add(intArg(2))) + end());

        setup.run(builder);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 0);
        ASSERT_EQ(setup.mWorld.mDropped.size(), 1u); // the second has nothing left
        EXPECT_EQ(setup.mWorld.mDropped[0].mCount, 4);
    }

    TEST(FalloutScriptSpawnCommandsTest, aDroppedItemKeepsItsConditionAndItsModsOneStackAtATime)
    {
        Runner setup;
        setup.mActors.addItem(guard, pistol, 3, 1000, 0);
        setup.mActors.addItem(guard, pistol, 2, 400, 5);
        setup.mActors.addItem(guard, pistol, 1, 650, 0);
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(pistol);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpDrop, arguments(2).add(refArg(item)).add(intArg(5))) + end());

        setup.run(builder);

        // The best are dropped first, and the copies that differ are not made into one stack
        ASSERT_EQ(setup.mWorld.mDropped.size(), 3u);
        EXPECT_EQ(setup.mWorld.mDropped[0].mCount, 3);
        EXPECT_EQ(setup.mWorld.mDropped[0].mCondition, 1000);
        EXPECT_EQ(setup.mWorld.mDropped[0].mMods, 0);
        EXPECT_EQ(setup.mWorld.mDropped[1].mCount, 1);
        EXPECT_EQ(setup.mWorld.mDropped[1].mCondition, 650);
        EXPECT_EQ(setup.mWorld.mDropped[2].mCount, 1);
        EXPECT_EQ(setup.mWorld.mDropped[2].mCondition, 400);
        EXPECT_EQ(setup.mWorld.mDropped[2].mMods, 5);
        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 1);
    }

    TEST(FalloutScriptSpawnCommandsTest, theItemThatDropsItselfKeepsItsCondition)
    {
        Runner setup;
        setup.mActors.addItem(guard, pistol, 1, 300, 2);
        setup.mWorld.mHolders[pistol] = guard;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpDropMe, arguments(0)) + end());

        setup.run(builder, pistol);

        ASSERT_EQ(setup.mWorld.mDropped.size(), 1u);
        EXPECT_EQ(setup.mWorld.mDropped[0].mCondition, 300);
        EXPECT_EQ(setup.mWorld.mDropped[0].mMods, 2);
    }

    TEST(FalloutScriptSpawnCommandsTest, aDropThatTheWorldDoesNotTakeLeavesTheItemsWithTheHolder)
    {
        Runner setup;
        setup.mActors.addItem(guard, pistol, 2, 400, 1);
        setup.mWorld.mRefuseDrops = true;
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(pistol);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpDrop, arguments(2).add(refArg(item)).add(intArg(2))) + end());

        setup.run(builder);

        EXPECT_THAT(setup.mWorld.mDropped, IsEmpty());
        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 2);
        // and with the condition they had
        ASSERT_EQ(setup.mActors.conditions(guard, pistol).size(), 1u);
        EXPECT_EQ(setup.mActors.conditions(guard, pistol)[0].mCondition, 400);
        EXPECT_EQ(setup.mActors.conditions(guard, pistol)[0].mMods, 1);
    }

    TEST(FalloutScriptSpawnCommandsTest, itemsOfOneKindAreMovedAndTheRestStay)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 }, { rifle, 1 }, { junk, 5 } };
        setup.mActorRecords.mItems[crate] = {};
        setup.mWorld.mTypes = { { pistol, 40 }, { rifle, 40 }, { junk, 31 } };
        ScriptBuilder builder;
        const std::uint16_t destination = builder.form(crate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpRemoveAllTypedItems,
                arguments(4).add(refArg(destination)).add(intArg(1)).add(intArg(0)).add(intArg(40)))
            + end());

        setup.run(builder);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 0);
        EXPECT_EQ(setup.mActors.itemCount(guard, rifle), 0);
        EXPECT_EQ(setup.mActors.itemCount(guard, junk), 5);
        EXPECT_EQ(setup.mActors.itemCount(crate, pistol), 2);
        EXPECT_EQ(setup.mActors.itemCount(crate, rifle), 1);
        EXPECT_EQ(setup.mActors.itemCount(crate, junk), 0);
    }

    TEST(FalloutScriptSpawnCommandsTest, itemsInTheListOfExceptionsAreKept)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 }, { rifle, 1 } };
        setup.mActorRecords.mItems[crate] = {};
        setup.mWorld.mTypes = { { pistol, 40 }, { rifle, 40 } };
        setup.mFormListRecords.mLists[junkList] = { rifle };
        ScriptBuilder builder;
        const std::uint16_t list = builder.form(junkList);
        const std::uint16_t destination = builder.form(crate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpRemoveAllTypedItems,
                arguments(5).add(refArg(destination)).add(intArg(1)).add(intArg(0)).add(intArg(40)).add(refArg(list)))
            + end());

        setup.run(builder);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 0);
        EXPECT_EQ(setup.mActors.itemCount(guard, rifle), 1);
        EXPECT_EQ(setup.mActors.itemCount(crate, pistol), 2);
        EXPECT_EQ(setup.mActors.itemCount(crate, rifle), 0);
    }

    TEST(FalloutScriptSpawnCommandsTest, withoutATypeEveryItemIsMoved)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 }, { junk, 5 } };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpRemoveAllTypedItems, arguments(0)) + end());

        setup.run(builder);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 0);
        EXPECT_EQ(setup.mActors.itemCount(guard, junk), 0);
    }

    TEST(FalloutScriptSpawnCommandsTest, theConditionOfMovedItemsIsKept)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 } };
        setup.mActorRecords.mItems[crate] = {};
        setup.mActors.removeItem(guard, pistol, 1); // one of the two...
        setup.mActors.addItem(guard, pistol, 1, 400); // ...comes back worn
        ScriptBuilder builder;
        const std::uint16_t destination = builder.form(crate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpRemoveAllTypedItems, arguments(1).add(refArg(destination))) + end());

        setup.run(builder);

        const auto stacks = setup.mActors.conditions(crate, pistol);
        ASSERT_EQ(stacks.size(), 2u);
        EXPECT_EQ(stacks[0].mCondition, 1000);
        EXPECT_EQ(stacks[1].mCondition, 400);
    }

    TEST(FalloutScriptSpawnCommandsTest, anInventoryThatIsResetIsAskedOfTheRecordAgain)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 } };
        setup.mActors.removeItem(guard, pistol, 2);
        setup.mActors.addItem(guard, junk, 7);
        ASSERT_EQ(setup.mActors.itemCount(guard, pistol), 0);
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpResetInventory, arguments(0)) + end());

        setup.run(builder);

        EXPECT_EQ(setup.mActors.itemCount(guard, pistol), 2);
        EXPECT_EQ(setup.mActors.itemCount(guard, junk), 0);
        EXPECT_EQ(setup.mRespawn.inventoryGeneration(guard), 1);
    }

    TEST(FalloutScriptSpawnCommandsTest, aCharacterWithANewInventoryIsDressedFromItsRecordAgain)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { vest, 1 } };
        setup.mActorRecords.mStartingArmor[guard] = { vest };
        ASSERT_THAT(setup.mEquipment.armor(guard), ElementsAre(vest));
        // A looted character has nothing on
        setup.mActors.removeItem(guard, vest, 1);
        ASSERT_THAT(setup.mEquipment.armor(guard), IsEmpty());
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpResetInventory, arguments(0)) + end());

        setup.run(builder);

        EXPECT_EQ(setup.mActors.itemCount(guard, vest), 1);
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(vest));
    }

    TEST(FalloutScriptSpawnCommandsTest, zonesAndInteriorsAreToldToResetOrNot)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t zoneIndex = builder.form(zone);
        const std::uint16_t cellIndex = builder.form(cellA);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSetZoneRespawns, arguments(2).add(refArg(zoneIndex)).add(intArg(0)))
            + command(OpResetInterior, arguments(1).add(refArg(cellIndex))) + end());

        setup.run(builder);

        EXPECT_TRUE(setup.mRespawn.zoneNeverResets(zone, 0));
        EXPECT_TRUE(setup.mRespawn.cellResets(cellA, 0, true));

        ScriptBuilder again;
        const std::uint16_t zoneAgain = again.form(zone);
        again.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSetZoneRespawns, arguments(2).add(refArg(zoneAgain)).add(intArg(1))) + end());
        Runner other;
        other.mRespawn.setZoneNeverResets(zone, true);
        other.run(again);
        EXPECT_FALSE(other.mRespawn.zoneNeverResets(zone, FalloutScript::ZoneFlag::neverResets));
    }

    TEST(FalloutScriptSpawnCommandsTest, purgingTheCellBuffersIsPassedOn)
    {
        Runner setup;
        setup.run(oneCommand(OpPurgeCellBuffers, arguments(0)));
        EXPECT_EQ(setup.mWorld.mPurged, 1);
    }

    TEST(FalloutScriptSpawnCommandsTest, aNamedReferenceIsInACellByTheCellOfThatReference)
    {
        Runner setup;
        setup.mObjectWorld.mCells = { { guard, cellA }, { crate, cellB } };
        ScriptBuilder builder;
        const std::uint16_t a = builder.form(cellA);
        const std::uint16_t other = builder.form(crate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetInCellParam, arguments(2).add(refArg(a)).add(refArg(other))))
            + setTo(var('s', 2), callToken(OpGetInCellParam, arguments(2).add(refArg(a)).add(refArg(other))))
            + end());
        builder.variable(1, integerVariable, "other").variable(2, integerVariable, "again");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0); // the crate is in the other cell
        EXPECT_EQ(setup.variable(2), 0);
        // the calling reference is in the first
        ScriptBuilder own;
        const std::uint16_t cellIndex = own.form(cellA);
        const std::uint16_t self = own.form(guard);
        own.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetInCellParam, arguments(2).add(refArg(cellIndex)).add(refArg(self))))
            + end());
        own.variable(1, integerVariable, "own");
        Runner second;
        second.mObjectWorld.mCells = { { guard, cellA } };
        second.run(own);
        EXPECT_EQ(second.variable(1), 1);
    }

    TEST(FalloutScriptSpawnCommandsTest, theParentIsWhatTheHostSays)
    {
        Runner setup;
        setup.mWorld.mParents[guard] = crate;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetParentRef, arguments(0))) + end());
        builder.variable(1, integerVariable, "parent");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), static_cast<double>(crate));
    }

    TEST(FalloutScriptSpawnCommandsTest, theNumberOfItemsIsTheirTotal)
    {
        Runner setup;
        setup.mActorRecords.mItems[guard] = { { pistol, 2 }, { junk, 5 } };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetContainerInventoryCount, arguments(0))) + end());
        builder.variable(1, integerVariable, "count");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 7);
    }

    TEST(FalloutScriptSpawnCommandsTest, theValueOfAnItemIsSetForTheFormItStandsFor)
    {
        Runner setup;
        setup.mObjectWorld.mBases[crate] = crateBase;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetItemValue, arguments(1).add(intArg(250)), 1)
            + end());
        builder.form(crate);

        setup.run(builder);

        int value = 0;
        ASSERT_TRUE(setup.mItemValues.find(crateBase, value));
        EXPECT_EQ(value, 250);
    }

    TEST(FalloutScriptSpawnCommandsTest, namesAreTheMessagesThatScriptsGave)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t text = builder.form(message);
        const std::uint16_t cellIndex = builder.form(cellA);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetActorFullName, arguments(1).add(refArg(text)))
            + command(OpSetCellFullName, arguments(2).add(refArg(cellIndex)).add(refArg(text))) + end());

        setup.run(builder);

        FormId found = 0;
        ASSERT_TRUE(setup.mNames.find(guard, found));
        EXPECT_EQ(found, message);
        ASSERT_TRUE(setup.mNames.find(cellA, found));
        EXPECT_EQ(found, message);
    }

    TEST(FalloutScriptSpawnCommandsTest, theNameOfACharacterIsGivenToItsBaseRecordSoEveryReferenceOfItShowsIt)
    {
        Runner setup;
        constexpr FormId guardBase = 0x01000700;
        setup.mObjectWorld.mBases[guard] = guardBase;
        ScriptBuilder builder;
        const std::uint16_t text = builder.form(message);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetActorFullName, arguments(1).add(refArg(text))) + end());

        setup.run(builder);

        FormId found = 0;
        ASSERT_TRUE(setup.mNames.find(guardBase, found));
        EXPECT_EQ(found, message);
        EXPECT_FALSE(setup.mNames.find(guard, found));
    }

    TEST(FalloutScriptSpawnCommandsTest, anObjectIsMadeWithTheCountDistanceAndDirection)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(crateBase);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpPlaceAtMeHealthPercent,
                arguments(5).add(refArg(base)).add(floatArg(0.5)).add(intArg(3)).add(floatArg(64)).add(intArg(2)))
            + end());

        setup.run(builder);

        ASSERT_EQ(setup.mObjectWorld.mPlaced.size(), 1u);
        EXPECT_EQ(setup.mObjectWorld.mPlaced[0].mNear, guard);
        EXPECT_EQ(setup.mObjectWorld.mPlaced[0].mBase, crateBase);
        EXPECT_EQ(setup.mObjectWorld.mPlaced[0].mCount, 3);
        EXPECT_EQ(setup.mObjectWorld.mPlaced[0].mDistance, 64);
        EXPECT_EQ(setup.mObjectWorld.mPlaced[0].mDirection, 2);
    }

    TEST(FalloutScriptItemValuesTest, theValuesAndNamesSurviveASave)
    {
        FalloutScript::ItemValues values;
        values.set(pistol, 99);
        FalloutScript::NameOverrides names;
        names.set(guard, message);
        FalloutScript::SaveWriter out;
        values.write(out);
        names.write(out);

        FalloutScript::ItemValues loadedValues;
        FalloutScript::NameOverrides loadedNames;
        FalloutScript::SaveReader in(out.bytes());
        loadedValues.read(in, 1);
        loadedNames.read(in, 1);
        EXPECT_TRUE(in.atEnd());
        int value = 0;
        ASSERT_TRUE(loadedValues.find(pistol, value));
        EXPECT_EQ(value, 99);
        FormId found = 0;
        ASSERT_TRUE(loadedNames.find(guard, found));
        EXPECT_EQ(found, message);
        loadedValues.clear();
        loadedNames.clear();
        EXPECT_EQ(loadedValues.size(), 0u);
        EXPECT_EQ(loadedNames.size(), 0u);
    }
}
