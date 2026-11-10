#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/equipment.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
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
    constexpr std::uint16_t OpGetWeaponHealthPerc = 0x11F4;

    class FakeRecords : public FalloutScript::ActorRecords, public FalloutScript::ItemRecords
    {
    public:
        std::map<FormId, std::vector<std::pair<FormId, int>>> mItems;
        std::map<FormId, ItemInfo> mInfo;
        std::map<FormId, std::vector<FormId>> mStartingArmor;
        bool mStartingKnown = true;

        bool startingArmor(FormId actor, std::vector<FormId>& armor) override
        {
            if (!mStartingKnown)
                return false;
            armor = mStartingArmor[actor];
            return true;
        }

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
            mInfo[vest] = { ItemKind::Armor, upperBody, 0, 20, 3 };
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

    TEST(FalloutScriptEquipmentTest, the_revision_changes_when_armor_goes_on_or_off_and_not_when_nothing_changes)
    {
        Wardrobe setup;
        const std::uint32_t start = setup.mEquipment.revision(guard);
        EXPECT_EQ(setup.mEquipment.revision(guard), start);

        ASSERT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::Equipped);
        const std::uint32_t worn = setup.mEquipment.revision(guard);
        EXPECT_NE(worn, start);

        // what is worn already does not change what is drawn
        EXPECT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::AlreadyEquipped);
        EXPECT_EQ(setup.mEquipment.revision(guard), worn);

        ASSERT_TRUE(setup.mEquipment.unequip(guard, vest));
        const std::uint32_t off = setup.mEquipment.revision(guard);
        EXPECT_NE(off, worn);

        setup.mEquipment.clear();
        EXPECT_NE(setup.mEquipment.revision(guard), off);
    }

    TEST(FalloutScriptEquipmentTest, the_revision_of_a_character_ignores_weapons_and_the_clothes_of_others)
    {
        Wardrobe setup;
        constexpr FormId visitor = 0x01000101;
        setup.mState.addItem(visitor, vest, 1);
        setup.mState.addItem(visitor, pistol, 1);
        const std::uint32_t start = setup.mEquipment.revision(guard);

        // a weapon, in the hands of the character or of another, is no change of what the character wears
        ASSERT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        ASSERT_EQ(setup.mEquipment.equip(visitor, pistol), Equipment::Outcome::Equipped);
        // the clothes of another character are no change either
        ASSERT_EQ(setup.mEquipment.equip(visitor, vest), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.revision(guard), start);
        EXPECT_NE(setup.mEquipment.revision(visitor), setup.mEquipment.revision(0x01000102));

        ASSERT_EQ(setup.mEquipment.equip(guard, helmet), Equipment::Outcome::Equipped);
        EXPECT_NE(setup.mEquipment.revision(guard), start);
    }

    TEST(FalloutScriptEquipmentTest, a_weapon_that_goes_on_is_the_best_copy_unless_a_copy_is_asked_for)
    {
        Wardrobe setup;
        setup.mState.addItem(guard, pistol, 1, 400);
        setup.mState.addItem(guard, pistol, 1, 700);
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 1000);
        // Another copy in the hands is no change of what is held
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol, 400), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 400);
        EXPECT_TRUE(setup.mEquipment.isEquipped(guard, pistol, 400));
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, pistol, 700));
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::AlreadyEquipped);
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol, 400), Equipment::Outcome::AlreadyEquipped);
        // A copy that the character does not have is not put on
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol, 550), Equipment::Outcome::NotCarried);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 400);
        EXPECT_THAT(setup.mEquipment.takeChanges(), SizeIs(1));
    }

    TEST(FalloutScriptEquipmentTest, a_weapon_with_mods_is_a_copy_of_its_own)
    {
        Wardrobe setup;
        setup.mState.addItem(guard, pistol, 1, 1000, 2);
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        // The best copy of one condition is the one with fewer mods
        EXPECT_EQ(setup.mEquipment.equippedMods(guard, pistol), 0);
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol, 1000, 2), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.equippedMods(guard, pistol), 2);
        EXPECT_EQ(setup.mEquipment.equippedCopy(guard, pistol), FalloutScript::copyOf(1000, 2));
        EXPECT_TRUE(setup.mEquipment.isEquipped(guard, pistol, 1000, 2));
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, pistol, 1000, 0));
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol, 1000, 2), Equipment::Outcome::AlreadyEquipped);
        // A copy with other mods than the character has is not put on
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol, 1000, 4), Equipment::Outcome::NotCarried);
        // The copy that is on keeps its mods when it wears out
        EXPECT_TRUE(setup.mEquipment.setEquippedCondition(guard, pistol, 900));
        EXPECT_EQ(setup.mEquipment.equippedMods(guard, pistol), 2);
        bool found = false;
        for (const ActorState::ConditionStack& stack : setup.mState.conditions(guard, pistol))
            found = found || (stack.mCondition == 900 && stack.mMods == 2);
        EXPECT_TRUE(found);
        EXPECT_TRUE(setup.mEquipment.unequip(guard, pistol));
        EXPECT_EQ(setup.mEquipment.equippedMods(guard, pistol), 0);
    }

    TEST(FalloutScriptEquipmentTest, the_copy_that_is_on_wears_out_and_is_mended_in_its_holder_too)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);
        EXPECT_TRUE(setup.mEquipment.setEquippedCondition(guard, pistol, 930));
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 930);
        const std::vector<ActorState::ConditionStack> stacks = setup.mState.conditions(guard, pistol);
        ASSERT_EQ(stacks.size(), 1u);
        EXPECT_EQ(stacks[0].mCondition, 930);
        EXPECT_TRUE(setup.mEquipment.setEquippedCondition(guard, pistol, 5000));
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 1000);
        EXPECT_EQ(setup.mState.conditions(guard, pistol).front().mCondition, 1000);
        // What is not on does not wear
        EXPECT_FALSE(setup.mEquipment.setEquippedCondition(guard, rifle, 10));
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, rifle), -1);
    }

    TEST(FalloutScriptEquipmentTest, armor_keeps_the_condition_of_the_copy_that_is_worn)
    {
        Wardrobe setup;
        setup.mState.addItem(guard, vest, 1, 250);
        EXPECT_EQ(setup.mEquipment.equip(guard, vest, 250), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, vest), 250);
        EXPECT_TRUE(setup.mEquipment.setEquippedCondition(guard, vest, 100));
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, vest), 100);
        // The suit that covers the same place replaces it, and the vest is not worn with any condition any more
        setup.mEquipment.equip(guard, armoredSuit);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, vest), -1);
        setup.mEquipment.equip(guard, vest);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, vest), 1000);
    }

    TEST(FalloutScriptEquipmentTest, when_the_copy_that_is_on_is_taken_away_another_copy_is_on)
    {
        Wardrobe setup;
        setup.mState.addItem(guard, pistol, 1, 400);
        setup.mEquipment.equip(guard, pistol, 400);
        // A script takes the copy that is on
        EXPECT_EQ(setup.mState.removeItem(guard, pistol, 1, 400), 1);
        EXPECT_EQ(setup.mEquipment.weapon(guard), pistol);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 1000);
        EXPECT_EQ(setup.mState.removeItem(guard, pistol, 1, 1000), 1);
        EXPECT_EQ(setup.mEquipment.weapon(guard), 0u);
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
        EXPECT_DOUBLE_EQ(setup.mEquipment.damageThreshold(guard), 4);
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

    TEST(FalloutScriptEquipmentTest, an_item_that_is_taken_away_is_told_as_taken_off)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);
        setup.mEquipment.equip(guard, cartridge);
        setup.mEquipment.equip(guard, vest);
        setup.mEquipment.setWeaponDrawn(guard, true);
        setup.mEquipment.takeChanges();

        setup.mState.removeItem(guard, pistol, 1);
        setup.mState.removeItem(guard, cartridge, 1);
        setup.mState.removeItem(guard, vest, 1);
        // Whatever looks at the character finds it out
        EXPECT_THAT(setup.mEquipment.armor(guard), IsEmpty());

        const std::vector<Equipment::WornChange> changes = setup.mEquipment.takeChanges();
        ASSERT_EQ(changes.size(), 3u);
        std::set<FormId> items;
        for (const Equipment::WornChange& change : changes)
        {
            EXPECT_FALSE(change.mWorn);
            EXPECT_EQ(change.mActor, guard);
            items.insert(change.mItem);
        }
        EXPECT_THAT(items, UnorderedElementsAre(pistol, cartridge, vest));

        // A weapon that comes later is not out because of the one that went
        setup.mState.addItem(guard, pistol, 1);
        setup.mEquipment.equip(guard, pistol);
        EXPECT_FALSE(setup.mEquipment.weaponDrawn(guard));
    }

    TEST(FalloutScriptEquipmentTest, an_item_that_is_taken_away_is_told_even_when_nothing_asks_about_the_character)
    {
        Wardrobe setup;
        setup.mEquipment.equip(guard, pistol);
        setup.mEquipment.equip(guard, vest);
        setup.mEquipment.takeChanges();

        setup.mState.removeItem(guard, pistol, 1);
        setup.mState.removeItem(guard, vest, 1);

        const std::vector<Equipment::WornChange> changes = setup.mEquipment.takeChanges();
        ASSERT_EQ(changes.size(), 2u);
        for (const Equipment::WornChange& change : changes)
            EXPECT_FALSE(change.mWorn);
        EXPECT_THAT(setup.mEquipment.takeChanges(), IsEmpty());
    }

    TEST(FalloutScriptEquipmentTest, a_character_starts_in_the_armor_its_record_gives)
    {
        Wardrobe setup;
        setup.mRecords.mStartingArmor[guard] = { vest, helmet, ghost, stimpak };

        // Only armor that the character carries is worn, and none of it is an event: it was dressed from the start
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(vest, helmet));
        EXPECT_TRUE(setup.mEquipment.isEquipped(guard, vest));
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 25);
        EXPECT_DOUBLE_EQ(setup.mEquipment.upperBodyRating(guard), 20);
        EXPECT_THAT(setup.mEquipment.takeChanges(), IsEmpty());

        // A change of clothes starts from what was worn
        setup.mEquipment.equip(guard, jumpsuit);
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(helmet, jumpsuit));
        const std::vector<Equipment::WornChange> changes = setup.mEquipment.takeChanges();
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_EQ(changes[0].mItem, vest);
        EXPECT_FALSE(changes[0].mWorn);
    }

    TEST(FalloutScriptEquipmentTest, a_character_starts_in_the_first_of_the_armor_that_covers_the_same_parts)
    {
        Wardrobe setup;
        setup.mRecords.mStartingArmor[guard] = { jumpsuit, helmet, vest, armoredSuit };

        // As in the model it is shown in: what comes first keeps its place, what overlaps it is carried and not worn
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(jumpsuit, helmet));
        EXPECT_THAT(setup.mEquipment.takeChanges(), IsEmpty());
    }

    TEST(FalloutScriptEquipmentTest, what_was_put_on_before_the_host_could_tell_is_not_taken_off_by_the_start)
    {
        Wardrobe setup;
        setup.mRecords.mStartingArmor[guard] = { vest, helmet };
        setup.mRecords.mStartingKnown = false;

        EXPECT_EQ(setup.mEquipment.equip(guard, jumpsuit), Equipment::Outcome::Equipped);
        setup.mRecords.mStartingKnown = true;

        // The jumpsuit covers what the vest does, so the vest stays in the bag, and the helmet comes before it
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(helmet, jumpsuit));
        const std::vector<Equipment::WornChange> changes = setup.mEquipment.takeChanges();
        ASSERT_EQ(changes.size(), 1u);
        EXPECT_EQ(changes[0].mItem, jumpsuit);
        EXPECT_TRUE(changes[0].mWorn);
    }

    TEST(FalloutScriptEquipmentTest, a_character_the_host_can_not_tell_about_yet_is_asked_again)
    {
        Wardrobe setup;
        setup.mRecords.mStartingArmor[guard] = { vest };
        setup.mRecords.mStartingKnown = false;
        EXPECT_THAT(setup.mEquipment.armor(guard), IsEmpty());

        setup.mRecords.mStartingKnown = true;
        EXPECT_THAT(setup.mEquipment.armor(guard), ElementsAre(vest));

        // What was taken off does not go back on at the next look
        setup.mEquipment.unequip(guard, vest);
        EXPECT_THAT(setup.mEquipment.armor(guard), IsEmpty());
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

    TEST(FalloutScriptEquipmentWearTest, an_item_that_is_on_loses_health_a_fraction_of_a_tenth_at_a_time)
    {
        Wardrobe setup;
        setup.mRecords.mInfo[pistol].mHealth = 500;
        ASSERT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        std::vector<std::pair<int, int>> told;
        setup.mEquipment.setWearListener([&told](FormId, FormId, int before, int after) { told.emplace_back(before, after); });

        // 0.2 of 500 is 0.4 of a tenth of a percent: the third shot is the first that is felt
        EXPECT_EQ(setup.mEquipment.wear(guard, pistol, 0.2), 1000);
        EXPECT_EQ(setup.mEquipment.wear(guard, pistol, 0.2), 1000);
        EXPECT_EQ(setup.mEquipment.wear(guard, pistol, 0.2), 999);
        EXPECT_THAT(told, ElementsAre(std::make_pair(1000, 999)));
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, pistol), 999);
        EXPECT_EQ(setup.mState.conditions(guard, pistol).front().mCondition, 999);

        // What is not on, and what has no health, does not wear
        EXPECT_EQ(setup.mEquipment.wear(guard, rifle, 5), -1);
        setup.mRecords.mInfo[vest].mHealth = 0;
        ASSERT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::Equipped);
        EXPECT_EQ(setup.mEquipment.wear(guard, vest, 5), -1);

        // All of it goes: the weapon is at nothing and stays on until the host takes it off
        EXPECT_EQ(setup.mEquipment.wear(guard, pistol, 1000), 0);
        EXPECT_EQ(setup.mEquipment.wear(guard, pistol, 1), 0);
        EXPECT_EQ(told.back(), std::make_pair(999, 0));
    }

    TEST(FalloutScriptEquipmentWearTest, an_item_with_no_health_left_does_not_go_on_but_one_that_is_on_stays)
    {
        Wardrobe setup;
        setup.mRecords.mInfo[pistol].mHealth = 500;
        setup.mRecords.mInfo[vest].mHealth = 400;
        ASSERT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        ASSERT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::Equipped);
        setup.mEquipment.wear(guard, pistol, 500);
        setup.mEquipment.wear(guard, vest, 400);
        EXPECT_EQ(setup.mEquipment.equippedCondition(guard, vest), 0);
        EXPECT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::AlreadyEquipped);

        ASSERT_TRUE(setup.mEquipment.unequip(guard, pistol));
        ASSERT_TRUE(setup.mEquipment.unequip(guard, vest));
        EXPECT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Broken);
        EXPECT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::Broken);
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, pistol));
        EXPECT_FALSE(setup.mEquipment.isEquipped(guard, vest));
    }

    TEST(FalloutScriptEquipmentWearTest, armor_below_half_its_health_gives_less_rating_and_threshold)
    {
        Wardrobe setup;
        setup.mRecords.mInfo[vest].mHealth = 400;
        ASSERT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::Equipped);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 20);
        EXPECT_DOUBLE_EQ(setup.mEquipment.damageThreshold(guard), 3);

        // 0.75 of the health: a quarter of it is gone, which armor does not feel
        setup.mEquipment.wear(guard, vest, 100);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 20);
        // a quarter left: three quarters of the rating
        setup.mEquipment.wear(guard, vest, 200);
        EXPECT_NEAR(setup.mEquipment.armorRating(guard), 15, 1e-9);
        EXPECT_NEAR(setup.mEquipment.damageThreshold(guard), 3, 1e-9); // 3 * 0.75 = 2.25: the next whole number up
        EXPECT_NEAR(setup.mEquipment.upperBodyRating(guard), 15, 1e-9);
        EXPECT_EQ(setup.mEquipment.wornArmor(guard).front().mCondition, 250);
    }

    TEST(FalloutScriptEquipmentWearTest, fallout_3_armor_gives_less_all_the_way_down_to_a_fifth)
    {
        Wardrobe setup;
        FalloutScript::Wear::Settings fallout3;
        fallout3.mFallout3 = true;
        fallout3.mArmorBase = 0.2;
        fallout3.mArmorMax = 1.0;
        setup.mEquipment.setWearSettings([&fallout3]() -> const FalloutScript::Wear::Settings& { return fallout3; });
        setup.mRecords.mInfo[vest].mHealth = 400;
        ASSERT_EQ(setup.mEquipment.equip(guard, vest), Equipment::Outcome::Equipped);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 20);

        // 0.75 of the health: 0.8 of the rating (16), where New Vegas still gives the whole
        setup.mEquipment.wear(guard, vest, 100);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 16);
        // a half: 0.6 (12)
        setup.mEquipment.wear(guard, vest, 100);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 12);
        // broken: a fifth of the rating is left
        setup.mEquipment.wear(guard, vest, 1000);
        EXPECT_EQ(setup.mEquipment.wornArmor(guard).front().mCondition, 0);
        EXPECT_DOUBLE_EQ(setup.mEquipment.armorRating(guard), 4);
    }

    TEST(FalloutScriptEquipmentWearTest, items_are_given_with_a_share_of_their_health)
    {
        Wardrobe setup;
        setup.mRecords.mInfo[pistol].mHealth = 500;
        setup.mEquipment.addWithHealth(guard, pistol, 2, 0.25);
        setup.mEquipment.addWithHealth(guard, stimpak, 1, 0.25); // no health to share: whole
        setup.mEquipment.addWithHealth(guard, pistol, 1, 4.0); // more than whole is whole
        const std::vector<ActorState::ConditionStack> stacks = setup.mState.conditions(guard, pistol);
        ASSERT_EQ(stacks.size(), 2u);
        EXPECT_EQ(stacks[0].mCondition, 1000);
        EXPECT_EQ(stacks[0].mCount, 2);
        EXPECT_EQ(stacks[1].mCondition, 250);
        EXPECT_EQ(stacks[1].mCount, 2);
        ASSERT_EQ(setup.mState.conditions(guard, stimpak).size(), 1u);
        EXPECT_EQ(setup.mState.conditions(guard, stimpak).front().mCondition, 1000);
    }

    TEST(FalloutScriptEquipmentWearTest, the_commands_of_the_weapon_in_the_hands_follow_the_executable)
    {
        Wardrobe setup;
        setup.mRecords.mInfo[pistol].mHealth = 500;
        EXPECT_DOUBLE_EQ(setup.mEquipment.weaponPercent(guard), 100); // hands
        EXPECT_FALSE(setup.mEquipment.setWeaponPercent(guard, 50));
        ASSERT_EQ(setup.mEquipment.equip(guard, pistol), Equipment::Outcome::Equipped);

        EXPECT_TRUE(setup.mEquipment.setWeaponPercent(guard, 50));
        EXPECT_DOUBLE_EQ(setup.mEquipment.weaponPercent(guard), 50);
        // raising it never reaches the whole: it stops one point of health short (499 of 500)
        EXPECT_TRUE(setup.mEquipment.setWeaponPercent(guard, 100));
        EXPECT_NEAR(setup.mEquipment.weaponPercent(guard), 99.8, 1e-9);
        // a mod does reach it
        EXPECT_TRUE(setup.mEquipment.modWeaponPercent(guard, -60));
        EXPECT_NEAR(setup.mEquipment.weaponPercent(guard), 39.8, 1e-9);
        EXPECT_TRUE(setup.mEquipment.modWeaponPercent(guard, 80));
        EXPECT_DOUBLE_EQ(setup.mEquipment.weaponPercent(guard), 100);
        EXPECT_TRUE(setup.mEquipment.modWeaponPercent(guard, -500));
        EXPECT_DOUBLE_EQ(setup.mEquipment.weaponPercent(guard), 0);
    }

    TEST(FalloutScriptEquipmentCommandsTest, the_actor_value_commands_leave_the_health_of_a_weapon_to_the_equipment)
    {
        // The game registers the commands of the equipment first and those of the actor values after them, and a later
        // handler of a command takes the place of an earlier one
        Runner setup;
        FalloutScript::ActorValues values;
        FalloutScript::PlayerProgress progress;
        FalloutScript::addActorValueCommands(setup.mInterpreter, values, setup.mSetup.mState, progress);
        setup.mSetup.mRecords.mInfo[pistol].mHealth = 500;
        setup.mSetup.mEquipment.equip(guard, pistol);
        ASSERT_TRUE(setup.mSetup.mEquipment.setWeaponPercent(guard, 50));
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetWeaponHealthPerc, arguments(0))) + end());
        builder.variable(1, 0, "health");

        setup.run(builder);

        EXPECT_DOUBLE_EQ(setup.variable(1), setup.mSetup.mEquipment.weaponPercent(guard));
        EXPECT_LT(setup.variable(1), 100);
    }
}
