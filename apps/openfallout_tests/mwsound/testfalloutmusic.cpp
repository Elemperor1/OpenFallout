#include "apps/openfallout/mwsound/falloutmusic.hpp"

#include <algorithm>
#include <string>

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
    }
}
