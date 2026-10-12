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

    // The 32 bytes of the XPRM of Fallout: the bounds, the colour, a float for the way it is drawn and the type
    std::string primitive(float x, float y, float z, std::uint32_t type)
    {
        std::string data;
        for (const float value : { x, y, z, 0.f, 0.5f, 1.f, 0.1f })
            append<float>(data, value);
        append<std::uint32_t>(data, type);
        return subRecord("XPRM", data);
    }

    TEST(ESM4PrimitiveTest, the_volume_of_a_box_is_read)
    {
        const auto references = loadRecords<ESM4::Reference>("REFR",
            record("REFR", 0x801, valueSubRecord<std::uint32_t>("NAME", 0x900) + primitive(203.8f, 61.6f, 128.f, 1)));

        ASSERT_EQ(references.size(), 1u);
        ASSERT_TRUE(references[0].mPrimitive.has_value());
        const ESM4::Primitive& volume = *references[0].mPrimitive;
        EXPECT_EQ(volume.mType, ESM4::Primitive::Box);
        EXPECT_FLOAT_EQ(volume.mBounds[0], 203.8f);
        EXPECT_FLOAT_EQ(volume.mBounds[1], 61.6f);
        EXPECT_FLOAT_EQ(volume.mBounds[2], 128.f);
        EXPECT_FLOAT_EQ(volume.mColor[1], 0.5f);
        EXPECT_FLOAT_EQ(volume.mAlpha, 0.1f);
    }

    TEST(ESM4PrimitiveTest, a_reference_without_a_volume_has_none)
    {
        const auto references
            = loadRecords<ESM4::Reference>("REFR", record("REFR", 0x801, valueSubRecord<std::uint32_t>("NAME", 0x900)));

        ASSERT_EQ(references.size(), 1u);
        EXPECT_FALSE(references[0].mPrimitive.has_value());
    }

    TEST(ESM4PrimitiveTest, a_volume_of_another_size_is_skipped_and_the_records_after_it_are_read)
    {
        const auto references = loadRecords<ESM4::Reference>("REFR",
            record("REFR", 0x801, subRecord("XPRM", bytePattern(28, 3)) + valueSubRecord<std::uint32_t>("XCNT", 4))
                + record("REFR", 0x802, primitive(5.f, 5.f, 5.f, 2)));

        ASSERT_EQ(references.size(), 2u);
        EXPECT_FALSE(references[0].mPrimitive.has_value());
        EXPECT_EQ(references[0].mCount, 4);
        ASSERT_TRUE(references[1].mPrimitive.has_value());
        EXPECT_EQ(references[1].mPrimitive->mType, ESM4::Primitive::Sphere);
    }
}
