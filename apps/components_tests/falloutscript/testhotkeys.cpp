#include <components/falloutscript/hotkeys.hpp>
#include <components/falloutscript/savestate.hpp>

#include <gtest/gtest.h>

namespace
{
    using FalloutScript::FormId;
    using FalloutScript::HotKeys;

    constexpr FormId stimpak = 0x00015169;
    constexpr FormId pistol = 0x00004C46;
    constexpr FormId armor = 0x000D1FD3;

    TEST(FalloutScriptHotKeysTest, an_item_goes_in_a_slot_and_comes_out_of_it)
    {
        HotKeys keys;
        EXPECT_EQ(keys.item(0), 0u);
        EXPECT_TRUE(keys.assign(0, stimpak));
        EXPECT_EQ(keys.item(0), stimpak);
        EXPECT_EQ(keys.slotOf(stimpak), 0);
        keys.clear(0);
        EXPECT_EQ(keys.item(0), 0u);
        EXPECT_EQ(keys.slotOf(stimpak), -1);
    }

    TEST(FalloutScriptHotKeysTest, the_ammunition_swap_slot_takes_no_item)
    {
        HotKeys keys;
        EXPECT_FALSE(HotKeys::assignable(HotKeys::ammoSwapSlot));
        EXPECT_FALSE(keys.assign(HotKeys::ammoSwapSlot, stimpak));
        EXPECT_EQ(keys.item(HotKeys::ammoSwapSlot), 0u);
        // Nor does a slot that is not there, or no item
        EXPECT_FALSE(keys.assign(-1, stimpak));
        EXPECT_FALSE(keys.assign(HotKeys::slotCount, stimpak));
        EXPECT_FALSE(keys.assign(0, 0));
        EXPECT_EQ(keys.item(HotKeys::slotCount), 0u);
    }

    TEST(FalloutScriptHotKeysTest, an_item_has_one_slot)
    {
        HotKeys keys;
        keys.assign(0, stimpak);
        keys.assign(3, pistol);
        keys.assign(5, stimpak);
        EXPECT_EQ(keys.item(0), 0u);
        EXPECT_EQ(keys.item(5), stimpak);
        EXPECT_EQ(keys.item(3), pistol);
        // A slot that is given another item loses the first
        keys.assign(3, armor);
        EXPECT_EQ(keys.item(3), armor);
        EXPECT_EQ(keys.slotOf(pistol), -1);
        keys.clearItem(armor);
        EXPECT_EQ(keys.item(3), 0u);
        keys.reset();
        EXPECT_EQ(keys.item(5), 0u);
    }

    TEST(FalloutScriptHotKeysTest, a_saved_game_brings_back_the_slots)
    {
        HotKeys keys;
        keys.assign(0, stimpak);
        keys.assign(7, pistol);
        FalloutScript::SaveWriter out;
        FalloutScript::writeHotKeys(keys, out);

        HotKeys again;
        again.assign(2, armor);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readHotKeys(again, in, FalloutScript::hotKeysSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(again.item(0), stimpak);
        EXPECT_EQ(again.item(7), pistol);
        EXPECT_EQ(again.item(2), 0u);
    }

    TEST(FalloutScriptHotKeysTest, a_form_of_a_plugin_that_is_gone_leaves_its_slot_empty)
    {
        HotKeys keys;
        keys.assign(0, stimpak);
        keys.assign(4, pistol);
        FalloutScript::SaveWriter out;
        FalloutScript::writeHotKeys(keys, out);

        HotKeys again;
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes, [](FormId form) -> std::optional<FormId> {
            if (form == pistol)
                return std::nullopt;
            return form;
        });
        FalloutScript::readHotKeys(again, in, FalloutScript::hotKeysSectionVersion);
        EXPECT_EQ(again.item(0), stimpak);
        EXPECT_EQ(again.item(4), 0u);
    }

    TEST(FalloutScriptHotKeysTest, a_save_that_is_cut_short_is_an_error_and_changes_nothing)
    {
        HotKeys keys;
        keys.assign(0, stimpak);
        FalloutScript::SaveWriter out;
        FalloutScript::writeHotKeys(keys, out);
        std::string bytes = out.take();
        bytes.resize(bytes.size() - 6);

        HotKeys again;
        again.assign(2, armor);
        FalloutScript::SaveReader in(bytes);
        EXPECT_THROW(
            FalloutScript::readHotKeys(again, in, FalloutScript::hotKeysSectionVersion), FalloutScript::SaveError);
        EXPECT_EQ(again.item(2), armor);
    }
}
