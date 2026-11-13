#include "apps/openfallout/mwinput/sdltriggerdevice.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace OFInput
{
    namespace
    {
        using TriggerFeel::Command;
        using TriggerFeel::Resistance;
        using TriggerFeel::Situation;
        using TriggerFeel::Trigger;

        constexpr Resistance pistol{ 0.3f, 0.6f, 0.6f };

        // What the stand-in for SDL_GameControllerSendEffect was given, and what it answers
        std::vector<std::vector<std::uint8_t>> sReports;
        std::vector<int> sAnswers;

        int fakeSendEffect(SDL_GameController*, const void* data, int size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            sReports.emplace_back(bytes, bytes + size);
            if (sAnswers.empty())
                return 0;
            const int answer = sAnswers.front();
            sAnswers.erase(sAnswers.begin());
            return answer;
        }

        std::vector<std::uint8_t> report(const std::optional<Resistance>& resistance)
        {
            const auto effect = TriggerFeel::dualSenseEffect(Trigger::Right, resistance);
            return { effect.begin(), effect.end() };
        }

        // Never dereferenced: the stand-in takes the reports
        SDL_GameController* const somePad = reinterpret_cast<SDL_GameController*>(alignof(std::max_align_t));

        struct OFInputSdlTriggerDeviceTest : ::testing::Test
        {
            void SetUp() override
            {
                sReports.clear();
                sAnswers.clear();
            }
        };

        TEST_F(OFInputSdlTriggerDeviceTest, aRefusedResistanceIsNotTriedAgainButTheClearStillReachesThePad)
        {
            SdlTriggerDevice device(somePad, true, false, &fakeSendEffect);
            sAnswers = { -1 };
            device.setResistance(Trigger::Right, pistol);
            EXPECT_FALSE(device.supported());
            device.setResistance(Trigger::Right, pistol);
            EXPECT_EQ(sReports.size(), 1u);
            device.clear(Trigger::Right);
            ASSERT_EQ(sReports.size(), 2u);
            EXPECT_EQ(sReports.back(), report(std::nullopt));
        }

        TEST_F(OFInputSdlTriggerDeviceTest, aPadWithoutResistanceIsNeverSentAReport)
        {
            SdlTriggerDevice device(somePad, false, false, &fakeSendEffect);
            device.setResistance(Trigger::Right, pistol);
            device.clear(Trigger::Right);
            EXPECT_FALSE(device.supported());
            EXPECT_TRUE(sReports.empty());
        }

        /// Records the calls; refuses resistance after the first one when asked to
        struct FakeDevice final : TriggerDevice
        {
            bool mSupported = true;
            bool mRefuseResistance = false;
            int mSets = 0;
            int mClears = 0;

            bool supported() const override { return mSupported; }
            void setResistance(Trigger, const Resistance&) override
            {
                ++mSets;
                if (mRefuseResistance)
                    mSupported = false;
            }
            void clear(Trigger) override { ++mClears; }
            bool pulseSupported() const override { return false; }
            void pulse(Trigger, const TriggerFeel::Pulse&) override {}
        };

        Situation inPlay()
        {
            Situation situation;
            situation.mWeaponDrawn = true;
            return situation;
        }

        TEST(OFInputTriggerDriveTest, aPadThatRefusedTheResistanceIsClearedOnTheNextUpdate)
        {
            TriggerFeel::Planner planner;
            FakeDevice device;
            device.mRefuseResistance = true;
            driveTriggers(planner, device, inPlay(), pistol);
            EXPECT_EQ(device.mSets, 1);
            driveTriggers(planner, device, inPlay(), pistol);
            EXPECT_EQ(device.mSets, 1);
            EXPECT_EQ(device.mClears, 1);
            driveTriggers(planner, device, inPlay(), pistol);
            EXPECT_EQ(device.mClears, 1);
        }

        TEST(OFInputTriggerDriveTest, aPadWithoutResistanceIsNeverToldToResist)
        {
            TriggerFeel::Planner planner;
            FakeDevice device;
            device.mSupported = false;
            driveTriggers(planner, device, inPlay(), pistol);
            driveTriggers(planner, device, inPlay(), pistol);
            EXPECT_EQ(device.mSets, 0);
        }
    }
}
