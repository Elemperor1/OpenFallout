#include "apps/openfallout/mwsound/falloutmusic.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>

namespace OFSound
{
    namespace
    {
        /// A decoder of `size` bytes that count up, and that starts again when it is opened
        class CountingDecoder final : public SoundDecoder
        {
        public:
            CountingDecoder(std::size_t size, int* opened)
                : SoundDecoder(nullptr)
                , mSize(size)
                , mOpened(opened)
            {
            }

            void open(VFS::Path::NormalizedView) override
            {
                mPosition = 0;
                ++*mOpened;
            }
            void close() override {}
            std::string getName() override { return "counting"; }
            void getInfo(int* samplerate, ChannelConfig* chans, SampleType* type) override
            {
                *samplerate = 44100;
                *chans = ChannelConfig_Stereo;
                *type = SampleType_Int16;
            }
            size_t read(char* buffer, size_t bytes) override
            {
                const std::size_t count = std::min(bytes, mSize - mPosition);
                for (std::size_t i = 0; i < count; ++i)
                    buffer[i] = static_cast<char>((mPosition + i) % 100);
                mPosition += count;
                return count;
            }
            size_t getSampleOffset() override { return mPosition / 4; }

        private:
            std::size_t mSize;
            std::size_t mPosition = 0;
            int* mOpened;
        };

        TEST(OFSoundFalloutMusicTest, aLoopingDecoderStartsTheFileAgainAtItsEnd)
        {
            int opened = 0;
            auto inner = std::make_shared<CountingDecoder>(10, &opened);
            LoopingDecoder decoder(inner, VFS::Path::NormalizedView("music/a.mp3"));
            std::vector<char> buffer(25);
            EXPECT_EQ(decoder.read(buffer.data(), buffer.size()), 25u);
            // 0..9, 0..9, 0..4: the file was opened again twice
            for (std::size_t i = 0; i < buffer.size(); ++i)
                EXPECT_EQ(buffer[i], static_cast<char>(i % 10)) << i;
            EXPECT_EQ(opened, 2);
        }

        TEST(OFSoundFalloutMusicTest, aLoopingDecoderGivesUpOnAFileThatIsEmpty)
        {
            int opened = 0;
            auto inner = std::make_shared<CountingDecoder>(0, &opened);
            LoopingDecoder decoder(inner, VFS::Path::NormalizedView("music/a.mp3"));
            std::vector<char> buffer(16);
            EXPECT_EQ(decoder.read(buffer.data(), buffer.size()), 0u);
            EXPECT_EQ(opened, 0);
        }

        TEST(OFSoundFalloutMusicTest, aLoopingDecoderPassesTheInfoOn)
        {
            int opened = 0;
            auto inner = std::make_shared<CountingDecoder>(4, &opened);
            LoopingDecoder decoder(inner, VFS::Path::NormalizedView("music/a.mp3"));
            int rate = 0;
            ChannelConfig channels = ChannelConfig_Mono;
            SampleType type = SampleType_UInt8;
            decoder.getInfo(&rate, &channels, &type);
            EXPECT_EQ(rate, 44100);
            EXPECT_EQ(channels, ChannelConfig_Stereo);
            EXPECT_EQ(type, SampleType_Int16);
            EXPECT_EQ(decoder.getName(), "counting");
        }

        std::vector<std::string> names(const std::vector<VFS::Path::Normalized>& files)
        {
            std::vector<std::string> result;
            for (const auto& file : files)
                result.emplace_back(file.value());
            return result;
        }

        TEST(OFSoundFalloutMusicTest, aFolderIsTheMp3FilesInIt)
        {
            TestingOpenMW::VFSTestFile a("a");
            const auto vfs = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("music/explore/b.mp3"), &a },
                { VFS::Path::NormalizedView("music/explore/a.mp3"), &a },
                { VFS::Path::NormalizedView("music/explore/notes.txt"), &a },
                { VFS::Path::NormalizedView("music/explore/loop.wav"), &a },
                { VFS::Path::NormalizedView("music/explore/finale/c.mp3"), &a },
                { VFS::Path::NormalizedView("music/battle/d.mp3"), &a } });
            // The files of the folder itself, by name; not those of the folders below it or of other folders
            EXPECT_EQ(names(falloutMusicFiles("explore\\", *vfs)),
                (std::vector<std::string>{ "music/explore/a.mp3", "music/explore/b.mp3" }));
            EXPECT_EQ(names(falloutMusicFiles("Battle\\", *vfs)), (std::vector<std::string>{ "music/battle/d.mp3" }));
            EXPECT_TRUE(falloutMusicFiles("dungeon\\", *vfs).empty());
        }

        TEST(OFSoundFalloutMusicTest, aFileIsFoundInTheMusicFolder)
        {
            TestingOpenMW::VFSTestFile a("a");
            const auto vfs = TestingOpenMW::createTestVFS({ { VFS::Path::NormalizedView("music/special/death.mp3"), &a },
                { VFS::Path::NormalizedView("music/tenpenny/lobby.mp3"), &a } });
            EXPECT_EQ(names(falloutMusicFiles("special\\death.mp3", *vfs)),
                (std::vector<std::string>{ "music/special/death.mp3" }));
            // A wav that was converted to an mp3 is found as the mp3
            EXPECT_EQ(names(falloutMusicFiles("tenpenny\\lobby.wav", *vfs)),
                (std::vector<std::string>{ "music/tenpenny/lobby.mp3" }));
            EXPECT_TRUE(falloutMusicFiles("special\\success.mp3", *vfs).empty());
            EXPECT_TRUE(falloutMusicFiles("", *vfs).empty());
        }

        /// What the mixer needs of the sound manager, with the tracks that are playing in a set
        struct FakeSound
        {
            struct Started
            {
                std::string mFile;
                float mGain;
                float mFade;
                bool mLoop;
            };
            std::vector<Started> mStarted;
            std::set<OFBase::SoundManager::MusicTrackId> mPlaying;
            std::vector<std::pair<OFBase::SoundManager::MusicTrackId, float>> mFaded;
            bool mCannotStart = false;
            /// A fade takes time: the faded tracks play until `finishFades`
            bool mFadesTakeTime = false;
            std::set<OFBase::SoundManager::MusicTrackId> mStopping;
            void finishFades()
            {
                for (const auto id : mStopping)
                    mPlaying.erase(id);
                mStopping.clear();
            }

            OFBase::SoundManager::MusicTrackId startMusicTrack(VFS::Path::NormalizedView file, float gain, float fade, bool loop)
            {
                if (mCannotStart)
                    return 0;
                mStarted.push_back({ std::string(file.value()), gain, fade, loop });
                const auto id = static_cast<OFBase::SoundManager::MusicTrackId>(mStarted.size());
                mPlaying.insert(id);
                return id;
            }
            bool isMusicTrackPlaying(OFBase::SoundManager::MusicTrackId id) const { return mPlaying.count(id) != 0; }
            void fadeMusicTrack(OFBase::SoundManager::MusicTrackId id, float gain, float, bool stopAtEnd)
            {
                mFaded.emplace_back(id, gain);
                if (stopAtEnd)
                {
                    if (mFadesTakeTime)
                        mStopping.insert(id);
                    else
                        mPlaying.erase(id);
                }
            }
        };

        MusicMixer::Stem stemOf(const char* file, float gain, bool loop)
        {
            MusicMixer::Stem stem;
            stem.mFile = VFS::Path::Normalized(file);
            stem.mGain = gain;
            stem.mLoop = loop;
            return stem;
        }

        TEST(OFSoundFalloutMusicTest, theMixerStartsTheStemsThatAreWantedWithTheirLoopAndFade)
        {
            FakeSound sound;
            MusicMixer mixer;
            mixer.update(sound, { stemOf("music/a.mp3", 0.5f, true), stemOf("music/b.mp3", 0.f, false) }, 6.f);
            ASSERT_EQ(sound.mStarted.size(), 2u);
            EXPECT_EQ(sound.mStarted[0].mFile, "music/a.mp3");
            EXPECT_TRUE(sound.mStarted[0].mLoop);
            EXPECT_FALSE(sound.mStarted[1].mLoop);
            EXPECT_FLOAT_EQ(sound.mStarted[0].mFade, 6.f);
            EXPECT_EQ(mixer.size(), 2u);
            EXPECT_TRUE(mixer.playing(sound));
            // asked again, nothing starts twice
            mixer.update(sound, { stemOf("music/a.mp3", 0.5f, true), stemOf("music/b.mp3", 0.f, false) }, 6.f);
            EXPECT_EQ(sound.mStarted.size(), 2u);
        }

        TEST(OFSoundFalloutMusicTest, theMixerFadesAStemToANewGainAndOutWhenItIsNotWanted)
        {
            FakeSound sound;
            MusicMixer mixer;
            mixer.update(sound, { stemOf("music/a.mp3", 0.f, true), stemOf("music/b.mp3", 0.5f, true) }, 1.f);
            mixer.update(sound, { stemOf("music/a.mp3", 0.7f, true) }, 1.f);
            // b is faded out and stopped, a goes to its new gain
            ASSERT_EQ(sound.mFaded.size(), 2u);
            EXPECT_EQ(sound.mFaded[0].first, 1u);
            EXPECT_FLOAT_EQ(sound.mFaded[0].second, 0.7f);
            EXPECT_EQ(sound.mFaded[1].first, 2u);
            EXPECT_FLOAT_EQ(sound.mFaded[1].second, 0.f);
            EXPECT_EQ(mixer.size(), 1u);
        }

        TEST(OFSoundFalloutMusicTest, theMixerIsAudibleWhileAStemFadesOutThoughItIsNotPlayingAStemAnyMore)
        {
            FakeSound sound;
            sound.mFadesTakeTime = true;
            MusicMixer mixer;
            mixer.update(sound, { stemOf("music/a.mp3", 0.5f, true) }, 3.f);
            EXPECT_TRUE(mixer.audible(sound));
            // nothing is wanted now: the stem is the mixer's no more, but the fade is heard
            mixer.update(sound, {}, 3.f);
            EXPECT_EQ(mixer.size(), 0u);
            EXPECT_TRUE(mixer.audible(sound));
            sound.finishFades();
            EXPECT_FALSE(mixer.audible(sound));
        }

        TEST(OFSoundFalloutMusicTest, stoppingTheMixerIsAudibleUntilTheFadeIsOver)
        {
            FakeSound sound;
            sound.mFadesTakeTime = true;
            MusicMixer mixer;
            mixer.update(sound, { stemOf("music/a.mp3", 0.5f, true), stemOf("music/b.mp3", 0.5f, true) }, 1.f);
            mixer.stopAll(sound, 2.f);
            EXPECT_TRUE(mixer.audible(sound));
            sound.finishFades();
            EXPECT_FALSE(mixer.audible(sound));
        }

        TEST(OFSoundFalloutMusicTest, aStemThatPlayedToItsEndIsNotStartedAgainUntilTheMixerIsToldToRestart)
        {
            FakeSound sound;
            MusicMixer mixer;
            const std::vector<MusicMixer::Stem> wanted{ stemOf("music/a.mp3", 0.5f, false) };
            mixer.update(sound, wanted, 1.f);
            EXPECT_FALSE(mixer.finished());
            sound.mPlaying.clear();
            mixer.update(sound, wanted, 1.f);
            EXPECT_TRUE(mixer.finished());
            EXPECT_FALSE(mixer.playing(sound));
            // it does not start by itself
            mixer.update(sound, wanted, 1.f);
            EXPECT_EQ(sound.mStarted.size(), 1u);
            EXPECT_TRUE(mixer.finished());
            // a set that is picked again is played again
            mixer.restart();
            EXPECT_FALSE(mixer.finished());
            mixer.update(sound, wanted, 1.f);
            EXPECT_EQ(sound.mStarted.size(), 2u);
            EXPECT_FALSE(mixer.finished());
        }

        TEST(OFSoundFalloutMusicTest, aSetIsNotOverWhileAnyOfItsStemsPlaysOrWhenNoneCouldBeStarted)
        {
            FakeSound sound;
            MusicMixer mixer;
            const std::vector<MusicMixer::Stem> wanted{ stemOf("music/a.mp3", 0.5f, false), stemOf("music/b.mp3", 0.f, false) };
            mixer.update(sound, wanted, 1.f);
            // the stem that is heard ends while the other still plays
            sound.mPlaying.erase(1);
            mixer.update(sound, wanted, 1.f);
            EXPECT_FALSE(mixer.finished());
            sound.mPlaying.erase(2);
            mixer.update(sound, wanted, 1.f);
            EXPECT_TRUE(mixer.finished());
            // files that cannot be started are not an end that was played
            FakeSound broken;
            broken.mCannotStart = true;
            MusicMixer other;
            other.update(broken, wanted, 1.f);
            other.update(broken, wanted, 1.f);
            EXPECT_FALSE(other.finished());
        }

        TEST(OFSoundFalloutMusicTest, stoppingAndForgettingTheMixerClearsWhatEnded)
        {
            FakeSound sound;
            MusicMixer mixer;
            const std::vector<MusicMixer::Stem> wanted{ stemOf("music/a.mp3", 0.5f, false) };
            mixer.update(sound, wanted, 1.f);
            sound.mPlaying.clear();
            mixer.update(sound, wanted, 1.f);
            ASSERT_TRUE(mixer.finished());
            mixer.stopAll(sound, 1.f);
            EXPECT_FALSE(mixer.finished());
            mixer.update(sound, wanted, 1.f);
            sound.mPlaying.clear();
            mixer.update(sound, wanted, 1.f);
            ASSERT_TRUE(mixer.finished());
            mixer.forget();
            EXPECT_FALSE(mixer.finished());
            EXPECT_EQ(mixer.size(), 0u);
        }
    }
}
