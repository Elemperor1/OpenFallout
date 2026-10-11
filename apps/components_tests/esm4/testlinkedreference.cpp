#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadrefr.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string link(std::uint32_t keyword, std::uint32_t reference)
    {
        std::string data;
        append<std::uint32_t>(data, keyword);
        append<std::uint32_t>(data, reference);
        return subRecord("XLKR", data);
    }

    TEST(ESM4LinkedReferenceTest, a_link_of_fallout_names_the_reference_alone)
    {
        const auto references = loadRecords<ESM4::Reference>("REFR",
            record("REFR", 0x801,
                valueSubRecord<std::uint32_t>("NAME", 0x00000900) + valueSubRecord<std::uint32_t>("XLKR", 0x00000810)));

        ASSERT_EQ(references.size(), 1u);
        ASSERT_EQ(references[0].mLinkedRefs.size(), 1u);
        EXPECT_TRUE(references[0].mLinkedRefs[0].keyword.isZeroOrUnset());
        EXPECT_EQ(references[0].mLinkedRefs[0].reference.toUint32(), 0x810u);
    }

    TEST(ESM4LinkedReferenceTest, a_link_of_skyrim_has_a_keyword_in_front_of_the_reference)
    {
        const auto references = loadRecords<ESM4::Reference>(
            "REFR", record("REFR", 0x801, link(0x00000820, 0x00000810) + link(0, 0x00000811)));

        ASSERT_EQ(references.size(), 1u);
        ASSERT_EQ(references[0].mLinkedRefs.size(), 2u);
        EXPECT_EQ(references[0].mLinkedRefs[0].keyword.toUint32(), 0x820u);
        EXPECT_EQ(references[0].mLinkedRefs[0].reference.toUint32(), 0x810u);
        EXPECT_TRUE(references[0].mLinkedRefs[1].keyword.isZeroOrUnset());
        EXPECT_EQ(references[0].mLinkedRefs[1].reference.toUint32(), 0x811u);
    }

    TEST(ESM4LinkedReferenceTest, a_link_of_another_size_is_skipped)
    {
        const auto references = loadRecords<ESM4::Reference>("REFR",
            record("REFR", 0x801,
                subRecord("XLKR", bytePattern(6, 3)) + valueSubRecord<std::uint32_t>("XLKR", 0x00000810)));

        ASSERT_EQ(references.size(), 1u);
        ASSERT_EQ(references[0].mLinkedRefs.size(), 1u);
        EXPECT_EQ(references[0].mLinkedRefs[0].reference.toUint32(), 0x810u);
    }

    TEST(ESM4LinkedReferenceTest, a_placed_character_has_its_links_too)
    {
        const auto characters = loadRecords<ESM4::ActorCharacter>(
            "ACHR", record("ACHR", 0x802, valueSubRecord<std::uint32_t>("XLKR", 0x00000812)));

        ASSERT_EQ(characters.size(), 1u);
        ASSERT_EQ(characters[0].mLinkedRefs.size(), 1u);
        EXPECT_EQ(characters[0].mLinkedRefs[0].reference.toUint32(), 0x812u);
    }
}
