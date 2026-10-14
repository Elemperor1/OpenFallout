#include <components/esm4/loadtact.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    TEST(ESM4TalkingActivatorTest, keeps_the_name_the_model_the_script_and_the_voice)
    {
        const auto activators = loadRecords<ESM4::TalkingActivator>("TACT",
            record("TACT", 0x801,
                zString("EDID", "OFIntercom") + zString("FULL", "Intercom") + zString("MODL", "props\\intercom.nif")
                    + valueSubRecord<std::uint32_t>("SCRI", 0x00000810)
                    + valueSubRecord<std::uint32_t>("VNAM", 0x00000811)));

        ASSERT_EQ(activators.size(), 1u);
        EXPECT_EQ(activators[0].mEditorId, "OFIntercom");
        EXPECT_EQ(activators[0].mFullName, "Intercom");
        EXPECT_EQ(activators[0].mModel.getOriginal(), "props\\intercom.nif");
        EXPECT_EQ(activators[0].mScriptId.toUint32(), 0x810u);
        EXPECT_EQ(activators[0].mVoiceType.toUint32(), 0x811u);
    }

    TEST(ESM4TalkingActivatorTest, skips_a_subrecord_it_does_not_know)
    {
        const auto activators = loadRecords<ESM4::TalkingActivator>("TACT",
            record("TACT", 0x801,
                zString("EDID", "OFIntercom") + subRecord("MODB", bytePattern(4, 1))
                    + subRecord("ZZZZ", bytePattern(6, 3)) + valueSubRecord<std::uint32_t>("VNAM", 0x00000811)));

        ASSERT_EQ(activators.size(), 1u);
        EXPECT_EQ(activators[0].mVoiceType.toUint32(), 0x811u);
    }
}
