#include <components/esm4/loadclas.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    /// The DATA of a class of Fallout 3 and New Vegas: four tag skills, the flags, the services, the trainer
    std::string classData(std::int32_t secondSkill, std::uint32_t flags)
    {
        std::string data;
        append<std::int32_t>(data, 0x29); // tag skill 1
        append<std::int32_t>(data, secondSkill); // tag skill 2
        append<std::int32_t>(data, 0x2d); // tag skill 3
        append<std::int32_t>(data, -1); // tag skill 4 (none)
        append<std::uint32_t>(data, flags);
        append<std::uint32_t>(data, 0); // services
        data += bytePattern(4, 0);
        return data;
    }

    ESM4::Class load(const std::string& data)
    {
        const std::vector<ESM4::Class> records
            = loadRecords<ESM4::Class>("CLAS", record("CLAS", 5, zString("EDID", "Text of EDID") + subRecord("DATA", data)));
        return records.at(0);
    }

    // The guard flag is the second flag of the flags word at the 16th byte of DATA (Fallout3.exe 0x54D490), not a bit of
    // the second tag skill: a class that tags skill 0x26 or 0x2a (bit 1 set) is no guard
    TEST(ESM4ClassTest, readsTheFlagsAfterTheTagSkills)
    {
        EXPECT_EQ(load(classData(0x26, 2)).mFo3Flags, 2u);
        EXPECT_EQ(load(classData(0x26, 1)).mFo3Flags, 1u);
        EXPECT_EQ(load(classData(0x26, 3)).mFo3Flags, 3u);
    }

    TEST(ESM4ClassTest, aTaggedSkillIsNotAFlag)
    {
        EXPECT_EQ(load(classData(0x26, 0)).mFo3Flags & 2u, 0u);
        EXPECT_EQ(load(classData(0x2a, 0)).mFo3Flags & 2u, 0u);
    }

    TEST(ESM4ClassTest, aShortDataHasNoFlags)
    {
        EXPECT_EQ(load(std::string(16, '\x03')).mFo3Flags, 0u);
        EXPECT_EQ(load(std::string(8, '\x03')).mFo3Flags, 0u);
    }
}
