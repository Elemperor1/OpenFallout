#include <components/esm4/loadrefr.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    /// XLOC of Fallout 3 and New Vegas: 20 bytes, the level, three bytes of padding, the key, the flags and eight more
    std::string lockData(std::uint8_t level, std::uint32_t key, std::uint32_t flags)
    {
        std::string data;
        append<std::uint8_t>(data, level);
        data.append(3, '\0');
        append(data, key);
        append(data, flags);
        data.append(8, '\0');
        return data;
    }

    ESM4::Reference load(const std::string& lock)
    {
        const std::string data = valueSubRecord("NAME", std::uint32_t(0x10)) + subRecord("XLOC", lock);
        const std::vector<ESM4::Reference> records = loadRecords<ESM4::Reference>("REFR", record("REFR", 0x2001, data));
        EXPECT_EQ(records.size(), 1u);
        return records.empty() ? ESM4::Reference() : records[0];
    }

    TEST(ESM4LockTest, readsTheLevelTheKeyAndTheFlagsOfALock)
    {
        const ESM4::Reference locked = load(lockData(50, 0x00010077, 0x04));
        EXPECT_TRUE(locked.mIsLocked);
        EXPECT_EQ(static_cast<std::uint8_t>(locked.mLockLevel), 50);
        EXPECT_EQ(locked.mKey.toUint32(), 0x00010077u);
        EXPECT_EQ(locked.mLockFlags, 0x04u);
    }

    TEST(ESM4LockTest, aLockThatOnlyItsKeyOpensHasLevel255)
    {
        const ESM4::Reference locked = load(lockData(255, 0x00010077, 0));
        EXPECT_EQ(static_cast<std::uint8_t>(locked.mLockLevel), 255);
        EXPECT_EQ(locked.mLockFlags, 0u);
    }

    TEST(ESM4LockTest, aReferenceWithoutALockIsNotLocked)
    {
        const std::vector<ESM4::Reference> records
            = loadRecords<ESM4::Reference>("REFR", record("REFR", 0x2001, valueSubRecord("NAME", std::uint32_t(0x10))));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_FALSE(records[0].mIsLocked);
        EXPECT_EQ(records[0].mLockFlags, 0u);
    }
}
