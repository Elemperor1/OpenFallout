#include <gtest/gtest.h>

#include <components/esm/attr.hpp>
#include <components/esm3/defaultgmsts.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadskil.hpp>

#include "apps/openfallout/mwworld/esmstore.hpp"
#include "apps/openfallout/mwworld/placeholderrecords.hpp"

namespace OFWorld
{
    namespace
    {
        const ESM::RefId sPlayerId = ESM::RefId::stringRefId("Player");

        TEST(OFWorldPlaceholderRecordsTest, emptyStoreShouldLackPlayerRecord)
        {
            ESMStore store;
            EXPECT_TRUE(lacksPlayerRecord(store));
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldGiveStoreValidPlayerWithRaceAndClass)
        {
            ESMStore store;
            insertPlaceholderRecords(store);
            EXPECT_FALSE(lacksPlayerRecord(store));
            const ESM::NPC* player = store.get<ESM::NPC>().find(sPlayerId);
            EXPECT_NE(store.get<ESM::Race>().search(player->mRace), nullptr);
            EXPECT_NE(store.get<ESM::Class>().search(player->mClass), nullptr);
            EXPECT_NO_THROW(store.checkPlayer());
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldGivePlayerEverySkillAndAttribute)
        {
            ESMStore store;
            insertPlaceholderRecords(store);
            const ESM::NPC* player = store.get<ESM::NPC>().find(sPlayerId);
            EXPECT_EQ(player->mNpdt.mSkills.size(), static_cast<std::size_t>(ESM::Skill::Length));
            EXPECT_EQ(player->mNpdt.mAttributes.size(), static_cast<std::size_t>(ESM::Attribute::Length));
            for (int i = 0; i < ESM::Skill::Length; ++i)
                EXPECT_NE(store.get<ESM::Skill>().search(ESM::Skill::indexToRefId(i)), nullptr) << i;
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldGiveEveryMagicEffectARecord)
        {
            ESMStore store;
            insertPlaceholderRecords(store);
            for (int i = 0; i < ESM::MagicEffect::Length; ++i)
                EXPECT_NE(store.get<ESM::MagicEffect>().search(ESM::MagicEffect::indexToRefId(i)), nullptr) << i;
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldGiveEveryGlobalTheClockAndNewGameNeed)
        {
            ESMStore store;
            insertPlaceholderRecords(store);
            for (const char* name : { "gamehour", "timescale", "dayspassed", "day", "month", "year", "chargenstate" })
                EXPECT_NE(store.get<ESM::Global>().search(ESM::RefId::stringRefId(name)), nullptr) << name;
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldGiveEveryDefaultGameSettingItsTypeAndValue)
        {
            ESMStore store;
            insertPlaceholderRecords(store);
            for (std::size_t i = 0; i < ESM::DefaultGmsts::FloatCount; ++i)
            {
                const ESM::GameSetting* gmst
                    = store.get<ESM::GameSetting>().search(ESM::RefId::stringRefId(ESM::DefaultGmsts::Floats[i]));
                ASSERT_NE(gmst, nullptr) << ESM::DefaultGmsts::Floats[i];
                EXPECT_EQ(gmst->mValue.getType(), ESM::VT_Float);
                EXPECT_EQ(gmst->mValue.getFloat(), ESM::DefaultGmsts::FloatsDefaultValues[i]);
            }
            for (std::size_t i = 0; i < ESM::DefaultGmsts::IntCount; ++i)
            {
                const ESM::GameSetting* gmst
                    = store.get<ESM::GameSetting>().search(ESM::RefId::stringRefId(ESM::DefaultGmsts::Ints[i]));
                ASSERT_NE(gmst, nullptr) << ESM::DefaultGmsts::Ints[i];
                EXPECT_EQ(gmst->mValue.getType(), ESM::VT_Int);
                EXPECT_EQ(gmst->mValue.getInteger(), ESM::DefaultGmsts::IntsDefaultValues[i]);
            }
            for (std::size_t i = 0; i < ESM::DefaultGmsts::StringCount; ++i)
            {
                const ESM::GameSetting* gmst
                    = store.get<ESM::GameSetting>().search(ESM::RefId::stringRefId(ESM::DefaultGmsts::Strings[i]));
                ASSERT_NE(gmst, nullptr) << ESM::DefaultGmsts::Strings[i];
                EXPECT_EQ(gmst->mValue.getType(), ESM::VT_String);
            }
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldKeepRecordsThatAreAlreadyInStore)
        {
            ESMStore store;
            ESM::GameSetting gmst;
            gmst.mId = ESM::RefId::stringRefId("fSwimHeightScale");
            gmst.mRecordFlags = 0;
            gmst.mValue.setType(ESM::VT_Float);
            gmst.mValue.setFloat(7.f);
            store.insertStatic(gmst);

            insertPlaceholderRecords(store);

            const ESM::GameSetting* result = store.get<ESM::GameSetting>().find("fSwimHeightScale");
            EXPECT_EQ(result->mValue.getFloat(), 7.f);
        }

        TEST(OFWorldPlaceholderRecordsTest, insertShouldNotThrowWhenCalledTwice)
        {
            ESMStore store;
            insertPlaceholderRecords(store);
            EXPECT_NO_THROW(insertPlaceholderRecords(store));
        }
    }
}
