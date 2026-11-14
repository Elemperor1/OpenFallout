#include <components/esm4/loadweap.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
    void putFloat(std::vector<std::uint8_t>& bytes, std::size_t offset, float value)
    {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    // The DNAM of New Vegas is 204 bytes; the rumble of a shot is at 72 (left motor), 76 (right motor) and 80 (seconds)
    TEST(ESM4WeaponGameDataTest, readsTheRumbleOfAShot)
    {
        std::vector<std::uint8_t> bytes(204, 0);
        putFloat(bytes, 68, 9.f); // the action points of the attack, just before it
        putFloat(bytes, 72, 0.25f);
        putFloat(bytes, 76, 0.75f);
        putFloat(bytes, 80, 0.33f);
        putFloat(bytes, 84, 2.f); // the damage to the weapon, just after it

        const ESM4::Weapon::GameData data = ESM4::Weapon::decodeGameData(bytes);

        EXPECT_FLOAT_EQ(data.mOverrideActionPoints, 9.f);
        EXPECT_FLOAT_EQ(data.mRumbleLeftMotor, 0.25f);
        EXPECT_FLOAT_EQ(data.mRumbleRightMotor, 0.75f);
        EXPECT_FLOAT_EQ(data.mRumbleDuration, 0.33f);
        EXPECT_FLOAT_EQ(data.mOverrideDamageToWeaponMult, 2.f);
    }

    TEST(ESM4WeaponGameDataTest, aShortRecordHasNoRumble)
    {
        const ESM4::Weapon::GameData data = ESM4::Weapon::decodeGameData(std::vector<std::uint8_t>(72, 0xff));

        EXPECT_FLOAT_EQ(data.mRumbleLeftMotor, 0.f);
        EXPECT_FLOAT_EQ(data.mRumbleRightMotor, 0.f);
        EXPECT_FLOAT_EQ(data.mRumbleDuration, 0.f);
    }
}
