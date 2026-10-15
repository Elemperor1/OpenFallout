#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/equipment.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ActorState;
    using FalloutScript::Equipment;
    using FalloutScript::FormId;
    using FalloutScript::ItemInfo;
    using FalloutScript::ItemKind;
    using FalloutScript::ObjectScripts;
    using FalloutScript::BipedSlot::upperBody;
    namespace BlockType = FalloutScript::BlockType;
    namespace BipedSlot = FalloutScript::BipedSlot;

    constexpr FormId guard = 0x01000100;
    constexpr FormId scriptOfGuards = 0x01002000;

    constexpr FormId pistol = 0x0100A001;
    constexpr FormId rifle = 0x0100A002;
    constexpr FormId cartridge = 0x0100A003;
    constexpr FormId buckshot = 0x0100A004;
    constexpr FormId vest = 0x0100B001;
    constexpr FormId helmet = 0x0100B002;
    constexpr FormId jumpsuit = 0x0100B003;
    constexpr FormId armoredSuit = 0x0100B004;
    constexpr FormId stimpak = 0x0100C001;
    constexpr FormId ghost = 0x0100DEAD;

    constexpr std::uint32_t integerVariable = 1;

    constexpr std::uint16_t OpGetEquipped = 0x10B6;
    constexpr std::uint16_t OpEquipItem = 0x10EE;
    constexpr std::uint16_t OpUnequipItem = 0x10EF;

    class FakeRecords : public FalloutScript::ActorRecords, public FalloutScript::ItemRecords
    {
    public:
        std::map<FormId, std::vector<std::pair<FormId, int>>> mItems;
        std::map<FormId, ItemInfo> mInfo;

        bool initialItems(FormId holder, std::vector<std::pair<FormId, int>>& items) override
        {
            items = mItems[holder];
            return true;
        }

        bool initialFactions(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }

        bool itemInfo(FormId item, ItemInfo& info) override
        {
            const auto found = mInfo.find(item);
            if (found == mInfo.end())
                return false;
            info = found->second;
            return true;
        }

        FakeRecords()
        {
            mInfo[pistol] = { ItemKind::Weapon, 0, 5, 0, 0 };
            mInfo[rifle] = { ItemKind::Weapon, 0, 8, 0, 0 };
            mInfo[cartridge] = { ItemKind::Ammo, 0, 0, 0, 0 };
            mInfo[buckshot] = { ItemKind::Ammo, 0, 0, 0, 0 };
            mInfo[vest] = { ItemKind::Armor, upperBody, 0, 20, 2.5 };
            mInfo[helmet] = { ItemKind::Armor, BipedSlot::head | BipedSlot::hair, 0, 5, 1 };
            mInfo[jumpsuit] = { ItemKind::Armor, upperBody, 0, 3, 0 };
            mInfo[armoredSuit] = { ItemKind::Armor, upperBody | BipedSlot::head, 0, 40, 5 };
            mInfo[stimpak] = { ItemKind::Aid, 0, 0, 0, 0 };
        }
    };

    struct Wardrobe
    {
        FakeRecords mRecords;
        ActorState mState{ mRecords };
        Equipment mEquipment{ mRecords, mState };

        Wardrobe()
        {
            for (const FormId item :
                { pistol, rifle, cartridge, buckshot, vest, helmet, jumpsuit, armoredSuit, stimpak })
                mState.addItem(guard, item, 1);
        }
    };

    TEST(FalloutScriptEquipmentTest, a_character_holds_one_weapon_and_changes_it_for_another)
    {
        Wardrobe setup;

        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.weapon(guard), pistol);
        EXPECT_EQ(setup.mEquipment.weaponAnimationType(guard), 5u);
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::AlreadyEquipped);

        EXPECT_EQ(setup.mEquipment.equip(guard, rifle), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.weapon(guard), rifle);
        EXPECT_EQ(setup.mEquipment.weaponAnimationType(guard), 8u);
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, pistol));
        EXPECT_TRUE(setup.mEquipment.isEquipped(guard, rifle));
    }

    TEST(FalloutScriptEquipmentTest, ammunition_is_chosen_apart_from_the_weapon)
    {
        Wardrobe setup;

        EXPECT_EQ(setup.mEquipment.equip(guard, cartridge), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.ammo(guard), cartridge);

        EXPECT_EQ(setup.mEquipment.equip(guard, buckshot), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.ammo(guard), buckshot);
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, cartridge));
        EXPECT_EQ(setup.mEquipment.weapon(guard), pistol);
    }

    TEST(FalloutScriptEquipmentTest, armor_that_covers_the_same_part_of_the_body_replaces_what_was_worn)
    {
        Wardrobe setup;

        setup.mEquipment.equip(guard, vest);
        setup.mEquipment.equip(guard, helmet);
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(vest, helmet));

        setup.mEquipment.equip(guard, jumpsuit);
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(helmet, jumpsuit));

        // The suit covers the head as well as the upper body: both are replaced
        setup.mEquipment.equip(guard, armoredSuit);
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(armoredSuit));
    }

    TEST(FalloutScriptEquipmentTest, the_rating_and_threshold_are_those_of_what_is_worn)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, vest);
        setup.mEquipment.equip(guard, helmet);

        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 25);
        EXPECT_DOUBLE_EQ(setup.mEquipment.damageThreshold(guard), 3.5);
        EXPECT_DOUBLE_EQ(setup.mEquipment.upperBodyRating(guard), 20);

        setup.mEquipment.unequip(guard, vest);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 5);
        EXPECT_DOUBLE_EQ(setup.mEquipment.upperBodyRating(guard), 0);
    }

    TEST(FalloutScriptEquipmentTest, only_what_a_character_carries_and_can_be_worn_goes_on)
    {
        Wardrobe setup;

        EXPECT_EQ(setup.mEquipment.equip(guard, stimpak), Equipment::Outcome::NotEquippable);
        EXPECT_EQ(setup.mEquipment.equip(guard, ghost), Equipment::Outcome::NotEquippable);
        EXPECT_EQ(setup.mEquipment.equip(0, pistol), Equipment::Outcome::NotEquippable);

        setup.mRecords.mInfo[ghost] = { ItemKind::Weapon, 0, 1, 0, 0 };
        EXPECT_EQ(setup.mEquipment.equip(guard, ghost), Equipment::Outcome::NotCarried);
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, ghost));
    }

    TEST(FalloutScriptEquipmentTest, an_item_that_is_given_away_is_no_longer_worn)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);
        setup.mEquipment.equip(guard, vest);
        setup.mEquipment.setWeaponDrawn(guard, true);
        ASSERT_TRUE(setup.mEquipment.weaponDrawn(guard));

        setup.mState.removeItem(guard, pistol, 1);
        setup.mState.removeItem(guard, vest, 1);

        EXPECT_EQ(setup.mEquipment.weapon(guard), 0u);
        EXPECT_THAT(setup.mEquipment.armor(guard), IsEmpty());
        EXPECT_FALSE(setup.mEquipment.weaponDrawn(guard));
        EXPECT_EQ(setup.mEquipment.weaponAnimationType(guard), 0u);
    }

    TEST(FalloutScriptEquipmentTest, taking_off_tells_whether_the_item_was_on)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);

        EXPECT_TRUE(setup.mEquipment.unequip(guard, pistol));
        EXPECT_FALSE(setup.mEquipment.unequip(guard, pistol));
        EXPECT_FALSE(setup.mEquipment.unequip(guard, rifle));
        EXPECT_EQ(setup.mEquipment.weapon(guard), 0u);
    }

    TEST(FalloutScriptEquipmentTest, the_weapon_is_out_only_while_one_is_held)
    {
        Wardrobe setup;
        setup.mEquipment.setWeaponDrawn(guard, true);
        EXPECT_FALSE(setup.mEquipment.weaponDrawn(guard));

        setup.mEquipment.equip(guard, pistol);
        EXPECT_TRUE(setup.mEquipment.weaponDrawn(guard));
        setup.mEquipment.unequip(guard, pistol);
        EXPECT_FALSE(setup.mEquipment.weaponDrawn(guard));
    }

    TEST(FalloutScriptEquipmentTest, clearing_takes_everything_off)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);
        setup.mEquipment.equip(guard, vest);

        setup.mEquipment.clear();

        EXPECT_EQ(setup.mEquipment.weapon(guard), 0u);
        EXPECT_THAT(setup.mEquipment.armor(guard), IsEmpty());
    }

    TEST(FalloutScriptEquipmentTest, a_character_carries_the_weight_of_all_its_items)
    {
        Wardrobe setup;
        setup.mRecords.mInfo[pistol].mWeight = 3.5;
        setup.mRecords.mInfo[stimpak].mWeight = 0.5;
        setup.mState.addItem(guard, stimpak, 3);

        double weight = -1;
        ASSERT_TRUE(setup.mEquipment.inventoryWeight(guard, weight));
        // The pistol and four stimpaks; what weighs nothing and what the records do not know add nothing
        EXPECT_DOUBLE_EQ(weight, 3.5 + 4 * 0.5);

        setup.mState.addItem(guard, ghost, 5);
        ASSERT_TRUE(setup.mEquipment.inventoryWeight(guard, weight));
        EXPECT_DOUBLE_EQ(weight, 3.5 + 4 * 0.5);
    }

    TEST(FalloutScriptEquipmentTest, the_changes_of_what_is_worn_are_told_once)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);
        setup.mEquipment.equip(guard, rifle);
        setup.mEquipment.equip(guard, rifle);
        setup.mEquipment.unequip(guard, rifle);

        const std::vector<Equipment::WornChange> changes = setup.mEquipment.takeChanges();
        ASSERT_EQ(changes.size(), 4u);
        EXPECT_EQ(changes[0].mItem, pistol);
        EXPECT_TRUE(changes[0].mWorn);
        EXPECT_EQ(changes[1].mItem, pistol);
        EXPECT_FALSE(changes[1].mWorn);
        EXPECT_EQ(changes[2].mItem, rifle);
        EXPECT_TRUE(changes[2].mWorn);
        EXPECT_EQ(changes[3].mItem, rifle);
        EXPECT_FALSE(changes[3].mWorn);
        EXPECT_EQ(changes[3].mActor, guard);
        EXPECT_THAT(setup.mEquipment.takeChanges(), IsEmpty());
    }

    TEST(FalloutScriptEquipmentTest, armor_that_is_replaced_is_told_as_taken_off)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, vest);
        setup.mEquipment.equip(guard, helmet);
        setup.mEquipment.takeChanges();

        setup.mEquipment.equip(guard, armoredSuit);

        const std::vector<Equipment::WornChange> changes = setup.mEquipment.takeChanges();
        ASSERT_EQ(changes.size(), 3u);
        EXPECT_FALSE(changes[0].mWorn);
        EXPECT_FALSE(changes[1].mWorn);
        EXPECT_EQ(changes[2].mItem, armoredSuit);
        EXPECT_TRUE(changes[2].mWorn);
    }

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
        Wardrobe mSetup;

        Runner()
        {
            FalloutScript::addActorCommands(mInterpreter, mSetup.mState);
            FalloutScript::addEquipmentCommands(mInterpreter, mSetup.mEquipment);
        }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
            mObjects.add(guard, scriptOfGuards);
            mObjects.trigger(guard, BlockType::OnActivate);
        }

        double variable(std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(guard);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    Bytes itemArg(std::uint16_t item)
    {
        return arguments(1).add(refArg(item));
    }

    TEST(FalloutScriptEquipmentCommandsTest, a_script_equips_and_takes_off_what_the_character_carries)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(pistol);
        const std::uint16_t second = builder.form(rifle);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpEquipItem, itemArg(first))
            + setTo(var('s', 1), callToken(OpGetEquipped, itemArg(first))) + command(OpEquipItem, itemArg(second))
            + setTo(var('s', 2), callToken(OpGetEquipped, itemArg(first)))
            + setTo(var('s', 3), callToken(OpGetEquipped, itemArg(second))) + command(OpUnequipItem, itemArg(second))
            + setTo(var('s', 4), callToken(OpGetEquipped, itemArg(second))) + end());
        builder.variable(1, integerVariable, "pistolOn")
            .variable(2, integerVariable, "pistolAfter")
            .variable(3, integerVariable, "rifleOn")
            .variable(4, integerVariable, "rifleAfter");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
        EXPECT_EQ(setup.variable(3), 1);
        EXPECT_EQ(setup.variable(4), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }
}
