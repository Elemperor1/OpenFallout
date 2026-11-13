#include "apps/openfallout/mwsound/falloutsound.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include <components/esm4/loadsoun.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>

namespace OFSound
{
    namespace
    {
        bool contains(const std::vector<VFS::Path::Normalized>& files, std::string_view path)
        {
            return std::any_of(files.begin(), files.end(), [path](const auto& file) { return file.value() == path; });
        }

        // The 36 bytes of a SNDD the way the loader keeps them: the settings, then the curve
        ESM4::Sound makeRecord(std::uint8_t min, std::uint8_t max, std::int8_t shift, std::uint16_t flags,
            std::uint16_t staticAttenuation, std::uint32_t size)
        {
            ESM4::Sound record;
            record.mData.minAttenuation = min;
            record.mData.maxAttenuation = max;
            record.mData.freqAdjustment = shift;
            record.mData.flags = flags;
            record.mData.staticAttenuation = staticAttenuation;
            record.mExtra.attenuationPoint1 = 100;
            record.mExtra.attenuationPoint2 = 80;
            record.mExtra.attenuationPoint3 = 40;
            record.mExtra.attenuationPoint4 = 10;
            record.mExtra.attenuationPoint5 = 0;
            record.mExtra.priority = 77;
            record.mDataSize = size;
            return record;
        }

        TEST(OFSoundFalloutSoundTest, theDataOfARecordIsItsSettingsAndCurve)
        {
            const ESM4::Sound record = makeRecord(8, 30, -10, ESM4::Sound::Flag_Loop, 600, 36);
            const FalloutScript::SoundData data = falloutSoundData(record);
            EXPECT_EQ(data.mMinAttenuation, 8);
            EXPECT_EQ(data.mMaxAttenuation, 30);
            EXPECT_EQ(data.mFrequencyAdjustment, -10);
            EXPECT_TRUE(data.isLooping());
            EXPECT_FLOAT_EQ(data.minDistance(), 40.f);
            EXPECT_FLOAT_EQ(data.maxDistance(), 3000.f);
            EXPECT_EQ(data.mCurve[1], 80);
            EXPECT_EQ(data.mCurve[2], 40);
            EXPECT_EQ(data.mPriority, 77);
            EXPECT_NEAR(data.staticGain(), 0.5012f, 0.001f); // six decibels down
        }

        TEST(OFSoundFalloutSoundTest, aRecordWithOnlyTheSettingsKeepsTheDefaultCurve)
        {
            const ESM4::Sound record = makeRecord(2, 20, 0, 0, 0, 12);
            const FalloutScript::SoundData data = falloutSoundData(record);
            EXPECT_EQ(data.mMaxAttenuation, 20);
            EXPECT_EQ(data.mCurve[0], 100);
            EXPECT_EQ(data.mCurve[1], 50);
            EXPECT_EQ(data.mCurve[4], 0);
        }

        TEST(OFSoundFalloutSoundTest, aRecordWithNoDataIsTheDefaults)
        {
            const ESM4::Sound record = makeRecord(9, 9, 9, 0xFF, 9, 0);
            const FalloutScript::SoundData data = falloutSoundData(record);
            EXPECT_EQ(data.mMaxAttenuation, 0);
            EXPECT_FALSE(data.hasDistance());
        }

        TEST(OFSoundFalloutSoundTest, aFileIsFoundAsTheRecordNamesIt)
        {
            TestingOpenMW::VFSTestFile file("a");
            const auto vfs = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("sound/fx/step.wav"), &file } });
            const auto files = falloutSoundFiles("fx\\step.wav", *vfs);
            ASSERT_EQ(files.size(), 1u);
            EXPECT_EQ(files[0].value(), "sound/fx/step.wav");
        }

        TEST(OFSoundFalloutSoundTest, aWavThatWasConvertedToAnMp3IsFoundAsTheMp3)
        {
            TestingOpenMW::VFSTestFile file("a");
            const auto vfs = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("sound/fx/step.mp3"), &file } });
            const auto files = falloutSoundFiles("fx\\step.wav", *vfs);
            ASSERT_EQ(files.size(), 1u);
            EXPECT_EQ(files[0].value(), "sound/fx/step.mp3");
        }

        TEST(OFSoundFalloutSoundTest, aWavThatTheArchivesHaveOnlyAsOggIsFoundAsTheOgg)
        {
            TestingOpenMW::VFSTestFile file("a");
            const auto vfs
                = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("sound/fx/amb/~regions/house_lp.ogg"), &file } });
            const auto files = falloutSoundFiles("fx\\amb\\~regions\\house_lp.wav", *vfs);
            ASSERT_EQ(files.size(), 1u);
            EXPECT_EQ(files[0].value(), "sound/fx/amb/~regions/house_lp.ogg");
        }

        TEST(OFSoundFalloutSoundTest, aWavThatIsThereIsPlayedBeforeAnOggOfTheSameName)
        {
            TestingOpenMW::VFSTestFile file("a");
            const auto vfs = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("sound/fx/step.wav"), &file },
                { VFS::Path::NormalizedView("sound/fx/step.ogg"), &file } });
            const auto files = falloutSoundFiles("fx\\step.wav", *vfs);
            ASSERT_EQ(files.size(), 1u);
            EXPECT_EQ(files[0].value(), "sound/fx/step.wav");
        }

        TEST(OFSoundFalloutSoundTest, aFileThatIsNotThereGivesNothing)
        {
            const auto vfs = TestingOpenMW::createTestVFS({});
            EXPECT_TRUE(falloutSoundFiles("fx\\step.wav", *vfs).empty());
            EXPECT_TRUE(falloutSoundFiles("", *vfs).empty());
        }

        TEST(OFSoundFalloutSoundTest, aFolderGivesEverySoundFileInItAndBelowIt)
        {
            TestingOpenMW::VFSTestFile a("a"), b("b"), c("c"), text("t");
            const auto vfs = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("sound/fx/dog/bark1.wav"), &a },
                { VFS::Path::NormalizedView("sound/fx/dog/bark2.ogg"), &b },
                { VFS::Path::NormalizedView("sound/fx/dog/far/bark3.mp3"), &c },
                { VFS::Path::NormalizedView("sound/fx/dog/notes.txt"), &text },
                { VFS::Path::NormalizedView("sound/fx/cat/meow.wav"), &a } });
            // With and without the folder of the sounds in front, as the records have them
            for (const char* name : { "fx\\dog\\", "sound\\fx\\dog\\" })
            {
                const auto files = falloutSoundFiles(name, *vfs);
                EXPECT_EQ(files.size(), 3u) << name;
                EXPECT_TRUE(contains(files, "sound/fx/dog/bark1.wav"));
                EXPECT_TRUE(contains(files, "sound/fx/dog/bark2.ogg"));
                EXPECT_TRUE(contains(files, "sound/fx/dog/far/bark3.mp3"));
                EXPECT_FALSE(contains(files, "sound/fx/dog/notes.txt"));
            }
        }
    }
}
