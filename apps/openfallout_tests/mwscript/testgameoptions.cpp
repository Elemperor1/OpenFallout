#include <gtest/gtest.h>

#include <components/falloutscript/falldamage.hpp>

#include "apps/openfallout/mwscript/falloutscripts.hpp"
#include "apps/openfallout/mwworld/esmstore.hpp"

namespace
{
    // The difficulty and the hardcore mode of the Gameplay screen are values of the game being played: the rules keep
    // them in the switches that a save holds, and reset() (a new game or a load) takes them back to the start
    TEST(FalloutGameOptions, theDifficultyStartsAtNormalAndKeepsWhatItIsGiven)
    {
        OFWorld::ESMStore store;
        store.setUp();
        OFScript::FalloutScripts scripts(store, FalloutScript::Game::NewVegas, 0x14);

        EXPECT_EQ(scripts.gameDifficulty(), FalloutScript::FallDamage::normalDifficulty);
        scripts.setGameDifficulty(0);
        EXPECT_EQ(scripts.gameDifficulty(), 0);
        scripts.setGameDifficulty(4);
        EXPECT_EQ(scripts.gameDifficulty(), 4);
        // beyond the five levels is the nearest level
        scripts.setGameDifficulty(9);
        EXPECT_EQ(scripts.gameDifficulty(), 4);
        scripts.setGameDifficulty(-3);
        EXPECT_EQ(scripts.gameDifficulty(), 0);

        scripts.reset();
        EXPECT_EQ(scripts.gameDifficulty(), FalloutScript::FallDamage::normalDifficulty);
    }

    TEST(FalloutGameOptions, hardcoreIsOffAtTheStartAndOnlyNewVegasHasIt)
    {
        OFWorld::ESMStore store;
        store.setUp();
        OFScript::FalloutScripts vegas(store, FalloutScript::Game::NewVegas, 0x14);
        EXPECT_FALSE(vegas.hardcoreMode());
        vegas.setHardcoreMode(true);
        EXPECT_TRUE(vegas.hardcoreMode());
        vegas.setHardcoreMode(false);
        EXPECT_FALSE(vegas.hardcoreMode());
        vegas.setHardcoreMode(true);
        vegas.reset();
        EXPECT_FALSE(vegas.hardcoreMode());

        // Fallout 3 has no hardcore mode: the item of the screen changes nothing
        OFScript::FalloutScripts fallout3(store, FalloutScript::Game::Fallout3, 0x14);
        fallout3.setHardcoreMode(true);
        EXPECT_FALSE(fallout3.hardcoreMode());
    }

    // The row of the needs in the display (FalloutNV.exe 0x7739F0): the three needs are drawn clear while the mode is
    // off, and the status says whether it is on
    TEST(FalloutGameOptions, theNeedsRowIsClearWhileHardcoreIsOff)
    {
        OFWorld::ESMStore store;
        store.setUp();
        OFScript::FalloutScripts vegas(store, FalloutScript::Game::NewVegas, 0x14);

        OFScript::FalloutScripts::NeedsStatus off = vegas.needsStatus();
        EXPECT_FALSE(off.mHardcore);
        for (int need = 0; need < 3; ++need)
        {
            EXPECT_EQ(off.mTiles[need].mColor, 1);
            EXPECT_EQ(off.mTiles[need].mAlpha, 0);
            EXPECT_EQ(off.mLevels[need], 0);
            // (no stage records in the store: the meter ends at the default)
            EXPECT_EQ(off.mMost[need], 1000);
        }
        EXPECT_EQ(off.mCrippledParts, 0);

        vegas.setHardcoreMode(true);
        const OFScript::FalloutScripts::NeedsStatus on = vegas.needsStatus();
        EXPECT_TRUE(on.mHardcore);
        // a need at nothing is below the first threshold: clear
        EXPECT_EQ(on.mTiles[0].mAlpha, 0);
    }
}
