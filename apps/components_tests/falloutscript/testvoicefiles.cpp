#include <components/falloutscript/voicefiles.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using FalloutScript::parseVoiceFileName;
    using FalloutScript::VoiceFileName;
    using FalloutScript::VoiceIndex;

    TEST(FalloutScriptVoiceFilesTest, readsTheNameOfAVoiceFile)
    {
        VoiceFileName name;
        ASSERT_TRUE(
            parseVoiceFileName("sound/voice/fallout3.esm/maleadult01default/vmq01_vmq01greeting_000a1b2c_2.ogg", name));
        EXPECT_EQ(name.mPlugin, "fallout3.esm");
        EXPECT_EQ(name.mVoiceType, "maleadult01default");
        EXPECT_EQ(name.mText, "vmq01_vmq01greeting");
        EXPECT_EQ(name.mResponse, 0x000a1b2cu);
        EXPECT_EQ(name.mNumber, 2u);
        EXPECT_EQ(name.mExtension, "ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, takesBackslashesAndUpperCase)
    {
        VoiceFileName name;
        ASSERT_TRUE(parseVoiceFileName("Sound\\Voice\\FalloutNV.esm\\PlayerVoiceMale\\Topic_00ABCDEF_11.WAV", name));
        EXPECT_EQ(name.mPlugin, "falloutnv.esm");
        EXPECT_EQ(name.mVoiceType, "playervoicemale");
        EXPECT_EQ(name.mResponse, 0x00abcdefu);
        EXPECT_EQ(name.mNumber, 11u);
        EXPECT_EQ(name.mExtension, "wav");
    }

    TEST(FalloutScriptVoiceFilesTest, leavesOutWhatIsNotAVoiceFileOfTheNames)
    {
        VoiceFileName name;
        // not under the voice folder, no voice type folder, a folder deeper
        EXPECT_FALSE(parseVoiceFileName("sound/fx/a_00000001_1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/empty.txt", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/a_00000001_1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/deeper/a_00000001_1.ogg", name));
        // the oddities of the games: no number, a letter in it, a short or long id, no id
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_00000001.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_00000001_f1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_00000001_1_alt.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_0001_1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_000000001_1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_0000000g_1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/_1.ogg", name));
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_00000001_1", name));
        // a number that does not fit a byte
        EXPECT_FALSE(parseVoiceFileName("sound/voice/fallout3.esm/v/a_00000001_300.ogg", name));
    }

    TEST(FalloutScriptVoiceFilesTest, findsTheFileOfALineByItsResponseAndNumber)
    {
        VoiceIndex index;
        index.add("sound/voice/fallout3.esm/maleadult01default/q_greeting_00001234_1.ogg");
        index.add("sound/voice/fallout3.esm/maleadult01default/q_greeting_00001234_2.ogg");
        index.add("sound/voice/fallout3.esm/maleadult01default/q_greeting_00001234_1.lip");
        index.add("sound/voice/fallout3.esm/maleadult01default/q_other_00005678_1.ogg");

        EXPECT_EQ(index.size(), 3u);
        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01default", 0x1234, 1),
            "sound/voice/fallout3.esm/maleadult01default/q_greeting_00001234_1.ogg");
        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01default", 0x1234, 2),
            "sound/voice/fallout3.esm/maleadult01default/q_greeting_00001234_2.ogg");
        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01default", 0x5678, 1),
            "sound/voice/fallout3.esm/maleadult01default/q_other_00005678_1.ogg");
        // no such line, no such response, no such plugin
        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01default", 0x1234, 3), "");
        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01default", 0x9999, 1), "");
        EXPECT_EQ(index.find({ "falloutnv.esm" }, "maleadult01default", 0x1234, 1), "");
        EXPECT_EQ(index.find({}, "maleadult01default", 0x1234, 1), "");
    }

    TEST(FalloutScriptVoiceFilesTest, countsTheLowerBitsOfTheFormIdOfTheResponse)
    {
        VoiceIndex index;
        index.add("sound/voice/brokensteel.esm/malechild/q_t_00001234_1.ogg");

        // the form id of the response has the index of the plugin above the 24 bits that the name carries
        EXPECT_EQ(index.find({ "brokensteel.esm" }, "malechild", 0x0a001234, 1),
            "sound/voice/brokensteel.esm/malechild/q_t_00001234_1.ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, theVoiceTypeOfTheSpeakerComesFirst)
    {
        VoiceIndex index;
        index.add("sound/voice/falloutnv.esm/femaleadult01/q_t_00001234_1.ogg");
        index.add("sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.ogg");

        EXPECT_EQ(index.find({ "falloutnv.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.ogg");
        EXPECT_EQ(index.find({ "falloutnv.esm" }, "femaleadult01", 0x1234, 1),
            "sound/voice/falloutnv.esm/femaleadult01/q_t_00001234_1.ogg");
        // the voice type of the speaker has none: the line of another voice is another person's, so there is none
        EXPECT_EQ(index.find({ "falloutnv.esm" }, "robot", 0x1234, 1), "");
        // a speaker whose voice type is not known takes the first file there is
        EXPECT_EQ(index.find({ "falloutnv.esm" }, "", 0x1234, 1),
            "sound/voice/falloutnv.esm/femaleadult01/q_t_00001234_1.ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, anOggComesBeforeAWavOfTheSameLine)
    {
        VoiceIndex index;
        index.add("sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.wav");
        index.add("sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.mp3");
        index.add("sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.ogg");

        EXPECT_EQ(index.find({ "falloutnv.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, looksInThePluginsInTheOrderGiven)
    {
        VoiceIndex index;
        index.add("sound/voice/fallout3.esm/maleadult01/q_t_00001234_1.ogg");
        index.add("sound/voice/brokensteel.esm/maleadult01/q_t_00001234_1.ogg");

        // the plugin that has the response comes first, then the ones that override it
        EXPECT_EQ(index.find({ "brokensteel.esm", "fallout3.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/brokensteel.esm/maleadult01/q_t_00001234_1.ogg");
        EXPECT_EQ(index.find({ "fallout3.esm", "brokensteel.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/fallout3.esm/maleadult01/q_t_00001234_1.ogg");
        // a plugin that has no file for the line leaves it to the next
        EXPECT_EQ(index.find({ "pointlookout.esm", "fallout3.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/fallout3.esm/maleadult01/q_t_00001234_1.ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, theSameIdInTwoPluginsIsTwoFiles)
    {
        // 11 ids of Broken Steel are those of other responses of Fallout 3 in their lower digits
        VoiceIndex index;
        index.add("sound/voice/fallout3.esm/maleadult01/a_00001234_1.ogg");
        index.add("sound/voice/brokensteel.esm/maleadult01/b_00001234_1.ogg");

        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/fallout3.esm/maleadult01/a_00001234_1.ogg");
        EXPECT_EQ(index.find({ "brokensteel.esm" }, "maleadult01", 0x1234, 1),
            "sound/voice/brokensteel.esm/maleadult01/b_00001234_1.ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, takesAPathInAnyCaseAndGivesItInTheLowerCaseOfTheFileSystem)
    {
        VoiceIndex index;
        index.add("Sound\\Voice\\FalloutNV.esm\\MaleAdult01\\Q_T_00001234_1.OGG");

        EXPECT_EQ(index.find({ "FalloutNV.esm" }, "MaleAdult01", 0x1234, 1),
            "sound/voice/falloutnv.esm/maleadult01/q_t_00001234_1.ogg");
    }

    TEST(FalloutScriptVoiceFilesTest, forgetsWhatItHadWhenCleared)
    {
        VoiceIndex index;
        index.add("sound/voice/fallout3.esm/maleadult01/q_t_00001234_1.ogg");
        index.clear();

        EXPECT_EQ(index.size(), 0u);
        EXPECT_EQ(index.find({ "fallout3.esm" }, "maleadult01", 0x1234, 1), "");
    }
}
