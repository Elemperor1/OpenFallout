#include <components/esm4/loadtree.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string billboard(float width, float height)
    {
        std::string data;
        append(data, width);
        append(data, height);
        return subRecord("BNAM", data);
    }

    TEST(ESM4TreeTest, readsTheModelTheImageAndTheSizeOfTheBillboard)
    {
        const std::string data = zString("EDID", "TestOak") + zString("MODL", "Trees\\Oak.spt")
            + zString("ICON", "Trees\\Billboards\\Oak.dds") + subRecord("CNAM", bytePattern(32, 1))
            + billboard(768.f, 812.f) + subRecord("SNAM", bytePattern(4, 9));
        const std::vector<ESM4::Tree> result = loadRecords<ESM4::Tree>("TREE", record("TREE", 1, data));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mEditorId, "TestOak");
        EXPECT_EQ(result[0].mModel.getOriginal(), "Trees\\Oak.spt");
        EXPECT_EQ(result[0].mLeafTexture, "Trees\\Billboards\\Oak.dds");
        EXPECT_EQ(result[0].mBillboardWidth, 768.f);
        EXPECT_EQ(result[0].mBillboardHeight, 812.f);
    }

    TEST(ESM4TreeTest, aTreeWithoutBillboardSizeHasNone)
    {
        const std::vector<ESM4::Tree> result = loadRecords<ESM4::Tree>(
            "TREE", record("TREE", 1, zString("MODL", "Trees\\Shrub.spt") + zString("ICON", "Trees\\Shrub.dds")));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mBillboardWidth, 0.f);
        EXPECT_EQ(result[0].mBillboardHeight, 0.f);
    }

    TEST(ESM4TreeTest, aBillboardSizeOfAnotherLengthIsSkipped)
    {
        const std::vector<ESM4::Tree> result = loadRecords<ESM4::Tree>("TREE",
            record("TREE", 1, subRecord("BNAM", bytePattern(12, 1)) + zString("MODL", "Trees\\Shrub.spt")));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mBillboardWidth, 0.f);
        EXPECT_EQ(result[0].mModel.getOriginal(), "Trees\\Shrub.spt");
    }
}
