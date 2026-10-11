#include <components/esm4/dialoguecensus.hpp>
#include <components/esm4/loaddial.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/readerutils.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string falloutHeader()
    {
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        return versionedRecord("TES4", 0, subRecord("HEDR", hedr), 15);
    }

    /// A record with the 24 byte header of Fallout.
    std::string fallout(std::string_view type, std::uint32_t id, std::string_view data)
    {
        return versionedRecord(type, id, data, 15);
    }

    /// A top-level group with the 24 byte header of Fallout.
    std::string falloutTopGroup(std::string_view recordType, std::string_view children)
    {
        std::string result("GRUP");
        append<std::uint32_t>(result, static_cast<std::uint32_t>(24 + children.size()));
        result.append(recordType);
        append<std::int32_t>(result, 0); // top level group
        append<std::uint16_t>(result, 0); // stamp
        append<std::uint16_t>(result, 0);
        append<std::uint16_t>(result, 0); // version
        append<std::uint16_t>(result, 0);
        result.append(children);
        return result;
    }

    /// The group of the responses of a topic, with the 24 byte header of Fallout.
    std::string topicGroup(std::uint32_t topic, const std::string& children)
    {
        std::string result("GRUP");
        append<std::uint32_t>(result, static_cast<std::uint32_t>(24 + children.size()));
        append<std::uint32_t>(result, topic);
        append<std::int32_t>(result, 7); // children of a topic
        append<std::uint16_t>(result, 0); // stamp
        append<std::uint16_t>(result, 0);
        append<std::uint16_t>(result, 0); // version
        append<std::uint16_t>(result, 0);
        result.append(children);
        return result;
    }

    /// TRDT as Fallout writes it: 24 bytes, 20 without the flags, or 16 without the sound too.
    std::string responseData(std::uint32_t emotion, std::int32_t value, std::uint8_t number, std::uint32_t sound,
        std::uint8_t flags, std::size_t size = 24)
    {
        std::string data;
        append(data, emotion);
        append(data, value);
        append<std::uint32_t>(data, 0); // unused
        append<std::uint8_t>(data, number);
        data.append(3, '\0');
        if (size >= 20)
            append(data, sound);
        if (size >= 24)
        {
            append<std::uint8_t>(data, flags);
            data.append(3, '\0');
        }
        return subRecord("TRDT", data);
    }

    std::string infoData(std::uint8_t type, std::uint8_t nextSpeaker, std::uint16_t flags)
    {
        std::string data;
        append(data, type);
        append(data, nextSpeaker);
        append(data, flags);
        return subRecord("DATA", data);
    }

    /// Every sub-record an INFO of Fallout 3 and New Vegas can have, with two responses.
    std::string everySubRecord()
    {
        return infoData(2, 1, 0x0105) + valueSubRecord<std::uint32_t>("QSTI", 0x00010002)
            + valueSubRecord<std::uint32_t>("TPIC", 0x00010003) + valueSubRecord<std::uint32_t>("PNAM", 0x00010004)
            + valueSubRecord<std::uint32_t>("NAME", 0x00010005) + valueSubRecord<std::uint32_t>("NAME", 0x00010006)
            + responseData(5, -30, 1, 0x00010007, 1) + zString("NAM1", "First line") + zString("NAM2", "Notes one")
            + zString("NAM3", "Edits one") + valueSubRecord<std::uint32_t>("SNAM", 0x00010008)
            + valueSubRecord<std::uint32_t>("LNAM", 0x00010009) + responseData(1, 12, 2, 0, 0, 20)
            + zString("NAM1", "Second line") + zString("NAM2", "Notes two")
            + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010001))
            + subRecord("CTDA", conditionData(0x01, 1.f, 72, 0)) + valueSubRecord<std::uint32_t>("TCLT", 0x0001000a)
            + valueSubRecord<std::uint32_t>("TCLT", 0x0001000b) + valueSubRecord<std::uint32_t>("TCLF", 0x0001000c)
            + valueSubRecord<std::uint32_t>("TCFU", 0x0001000d) + valueSubRecord<std::uint32_t>("SNDD", 0x0001000e)
            + zString("RNAM", "Prompt") + valueSubRecord<std::uint32_t>("ANAM", 0x0001000f)
            + valueSubRecord<std::uint32_t>("KNAM", 0x00010010) + valueSubRecord<std::uint32_t>("DNAM", 4);
    }

    std::vector<ESM4::DialogInfo> loadInfos(
        const std::string& groupChildren, std::uint32_t topic = 0x00000123, std::size_t cutBytes = 0)
    {
        std::string plugin = falloutHeader() + topicGroup(topic, groupChildren);
        plugin.resize(plugin.size() - cutBytes);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.isFalloutFile());
        std::vector<ESM4::DialogInfo> result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        return result;
    }

    std::vector<ESM4::DialogInfo> loadInfo(const std::string& data)
    {
        return loadInfos(fallout("INFO", 0x10, data));
    }

    std::string infoFailure(const std::string& data)
    {
        try
        {
            loadInfo(data);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    std::vector<ESM4::Dialogue> loadTopics(const std::string& records)
    {
        const std::string plugin = falloutHeader() + falloutTopGroup("DIAL", records);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.isFalloutFile());
        std::vector<ESM4::Dialogue> result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        return result;
    }

    std::string topicFailure(const std::string& data)
    {
        try
        {
            loadTopics(fallout("DIAL", 0x20, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4DialogInfoFalloutTest, readsEverySubrecord)
    {
        const std::vector<ESM4::DialogInfo> infos = loadInfo(everySubRecord());

        ASSERT_EQ(infos.size(), 1u);
        const ESM4::DialogInfo& info = infos[0];
        EXPECT_EQ(info.mId.toUint32(), 0x10u);
        EXPECT_EQ(info.mDialType, 2);
        EXPECT_EQ(info.mNextSpeaker, ESM4::DialogInfo::Speaker_Self);
        EXPECT_EQ(info.mInfoFlags, 0x0105u);
        EXPECT_EQ(info.mQuest.toUint32(), 0x00010002u);
        // The group the record is in says the topic (TPIC, 0x00010003, is read for a record outside of one)
        EXPECT_EQ(info.mTopic.toUint32(), 0x123u);
        EXPECT_EQ(info.mPreviousInfo.toUint32(), 0x00010004u);
        ASSERT_EQ(info.mAddTopics.size(), 2u);
        EXPECT_EQ(info.mAddTopics[1].toUint32(), 0x00010006u);

        ASSERT_EQ(info.mResponses.size(), 2u);
        EXPECT_EQ(info.mResponses[0].mEmotionType, 5u);
        EXPECT_EQ(info.mResponses[0].mEmotionValue, -30);
        EXPECT_EQ(info.mResponses[0].mNumber, 1);
        EXPECT_EQ(info.mResponses[0].mSound.toUint32(), 0x00010007u);
        EXPECT_EQ(info.mResponses[0].mFlags, ESM4::DialogResponse::Flag_UseEmotionAnimation);
        EXPECT_EQ(info.mResponses[0].mText, "First line");
        EXPECT_EQ(info.mResponses[0].mNotes, "Notes one");
        EXPECT_EQ(info.mResponses[0].mEdits, "Edits one");
        EXPECT_EQ(info.mResponses[0].mSpeakerAnimation.toUint32(), 0x00010008u);
        EXPECT_EQ(info.mResponses[0].mListenerAnimation.toUint32(), 0x00010009u);
        EXPECT_EQ(info.mResponses[1].mEmotionType, 1u);
        EXPECT_EQ(info.mResponses[1].mNumber, 2);
        EXPECT_TRUE(info.mResponses[1].mSound.isZeroOrUnset());
        EXPECT_EQ(info.mResponses[1].mFlags, 0);
        EXPECT_EQ(info.mResponses[1].mText, "Second line");
        EXPECT_EQ(info.mResponses[1].mNotes, "Notes two");
        EXPECT_TRUE(info.mResponses[1].mEdits.empty());
        EXPECT_TRUE(info.mResponses[1].mSpeakerAnimation.isZeroOrUnset());
        // The first response is also where the older members are
        EXPECT_EQ(info.mResponse, "First line");
        EXPECT_EQ(info.mNotes, "Notes one");

        ASSERT_EQ(info.mConditions.size(), 2u);
        EXPECT_EQ(info.mConditions[0].functionIndex, 14u);
        EXPECT_EQ(info.mConditions[0].reference, 0x00010001u);
        EXPECT_EQ(info.mConditions[1].functionIndex, 72u);
        ASSERT_EQ(info.mChoices.size(), 2u);
        EXPECT_EQ(info.mChoices[0].toUint32(), 0x0001000au);
        EXPECT_EQ(info.mChoices[1].toUint32(), 0x0001000bu);
        ASSERT_EQ(info.mLinkedFrom.size(), 1u);
        EXPECT_EQ(info.mLinkedFrom[0].toUint32(), 0x0001000cu);
        ASSERT_EQ(info.mFollowUps.size(), 1u);
        EXPECT_EQ(info.mFollowUps[0].toUint32(), 0x0001000du);
        EXPECT_EQ(info.mSound.toUint32(), 0x0001000eu);
        EXPECT_EQ(info.mPrompt, "Prompt");
        EXPECT_EQ(info.mSpeaker.toUint32(), 0x0001000fu);
        EXPECT_EQ(info.mChallengeStat.toUint32(), 0x00010010u);
        EXPECT_EQ(info.mChallenge, 4u);
    }

    TEST(ESM4DialogInfoFalloutTest, takesTheTopicFromTheGroupItIsIn)
    {
        const std::vector<ESM4::DialogInfo> infos = loadInfos(
            fallout("INFO", 0x10, infoData(0, 0, 0)) + fallout("INFO", 0x11, infoData(0, 0, 0)), 0x00000123);

        ASSERT_EQ(infos.size(), 2u);
        EXPECT_EQ(infos[0].mTopic.toUint32(), 0x123u);
        EXPECT_EQ(infos[1].mTopic.toUint32(), 0x123u);
        EXPECT_EQ(infos[1].mId.toUint32(), 0x11u);
    }

    TEST(ESM4DialogInfoFalloutTest, theGroupIsTheTopicAndNotTheTopicSubrecord)
    {
        // The TPIC of the record names another topic than the group it is in
        const std::vector<ESM4::DialogInfo> infos = loadInfos(
            fallout("INFO", 0x10, infoData(0, 0, 0) + valueSubRecord<std::uint32_t>("TPIC", 0x00010003)), 0x00000123);

        ASSERT_EQ(infos.size(), 1u);
        EXPECT_EQ(infos[0].mTopic.toUint32(), 0x123u);
    }

    TEST(ESM4DialogInfoFalloutTest, readsAResponseOfTwentyBytes)
    {
        const std::vector<ESM4::DialogInfo> infos
            = loadInfo(responseData(3, 4, 7, 0x00010007, 0, 20) + zString("NAM1", "Hello") + infoData(0, 0, 0));

        ASSERT_EQ(infos.size(), 1u);
        ASSERT_EQ(infos[0].mResponses.size(), 1u);
        EXPECT_EQ(infos[0].mResponses[0].mNumber, 7);
        EXPECT_EQ(infos[0].mResponses[0].mSound.toUint32(), 0x00010007u);
        EXPECT_EQ(infos[0].mResponses[0].mFlags, 0);
    }

    TEST(ESM4DialogInfoFalloutTest, readsAResponseOfSixteenBytesWithoutASound)
    {
        const std::vector<ESM4::DialogInfo> infos
            = loadInfo(responseData(3, 4, 7, 0, 0, 16) + zString("NAM1", "Hello") + infoData(0, 0, 0));

        ASSERT_EQ(infos.size(), 1u);
        ASSERT_EQ(infos[0].mResponses.size(), 1u);
        EXPECT_EQ(infos[0].mResponses[0].mEmotionType, 3u);
        EXPECT_EQ(infos[0].mResponses[0].mEmotionValue, 4);
        EXPECT_EQ(infos[0].mResponses[0].mNumber, 7);
        EXPECT_TRUE(infos[0].mResponses[0].mSound.isZeroOrUnset());
    }

    TEST(ESM4DialogInfoFalloutTest, readsDataOfThreeBytesWithOneByteOfFlags)
    {
        const std::vector<ESM4::DialogInfo> infos = loadInfo(subRecord("DATA", std::string("\x03\x01\x44", 3)));

        ASSERT_EQ(infos.size(), 1u);
        EXPECT_EQ(infos[0].mDialType, 3);
        EXPECT_EQ(infos[0].mNextSpeaker, 1);
        EXPECT_EQ(infos[0].mInfoFlags, 0x44);
    }

    TEST(ESM4DialogInfoFalloutTest, textWithoutAResponseDataStartsTheFirstResponse)
    {
        const std::vector<ESM4::DialogInfo> infos = loadInfo(zString("NAM1", "Hello") + zString("NAM1", "Again"));

        ASSERT_EQ(infos.size(), 1u);
        ASSERT_EQ(infos[0].mResponses.size(), 1u);
        EXPECT_EQ(infos[0].mResponses[0].mText, "Again");
    }

    TEST(ESM4DialogInfoFalloutTest, keepsTheOrderOfSeveralConditionsWithTheirRunOn)
    {
        std::string data;
        for (std::uint32_t i = 0; i < 5; ++i)
            data += subRecord("CTDA", conditionData(i == 1 ? 1 : 0, static_cast<float>(i), 100 + i, 0));

        const std::vector<ESM4::DialogInfo> infos = loadInfo(data);

        ASSERT_EQ(infos.size(), 1u);
        ASSERT_EQ(infos[0].mConditions.size(), 5u);
        for (std::uint32_t i = 0; i < 5; ++i)
            EXPECT_EQ(infos[0].mConditions[i].functionIndex, 100 + i);
        EXPECT_EQ(infos[0].mConditions[0].runOn, 1u);
    }

    TEST(ESM4DialogInfoFalloutTest, rejectsSizesThatNoGameUses)
    {
        EXPECT_EQ(
            infoFailure(subRecord("QSTI", std::string(3, 'x'))), "ESM4::INFO::load - QSTI has an unexpected size");
        EXPECT_EQ(
            infoFailure(subRecord("TRDT", std::string(21, 'x'))), "ESM4::INFO::load - TRDT has an unexpected size");
        EXPECT_EQ(
            infoFailure(subRecord("DATA", std::string(5, 'x'))), "ESM4::INFO::load - DATA has an unexpected size");
        EXPECT_EQ(
            infoFailure(subRecord("DATA", std::string(2, 'x'))), "ESM4::INFO::load - DATA has an unexpected size");
        EXPECT_EQ(
            infoFailure(subRecord("DNAM", std::string(3, 'x'))), "ESM4::INFO::load - DNAM has an unexpected size");
        EXPECT_EQ(infoFailure(subRecord("NEXT", "x")), "ESM4::INFO::load - NEXT has an unexpected size");
    }

    TEST(ESM4DialogInfoFalloutTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(infoFailure(subRecord("ZZZZ", "1")), "ESM4::INFO::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4DialogInfoFalloutTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(infoFailure(infoData(0, 0, 0) + std::string(3, '\0')),
            "ESM4::INFO::load - record has unread bytes after its last sub-record");
    }

    TEST(ESM4DialogInfoFalloutTest, adjustsTheFormIdsToTheLoadOrder)
    {
        // The plugin is the third in the load order and has no masters, so its own records have its index
        const std::string plugin = falloutHeader()
            + topicGroup(0x02000123,
                fallout("INFO", 0x02000010,
                    valueSubRecord<std::uint32_t>("QSTI", 0x02000020)
                        + valueSubRecord<std::uint32_t>("TCLT", 0x02000021)
                        + valueSubRecord<std::uint32_t>("TCLT", 0)));
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        reader.setModIndex(2);
        std::vector<ESM4::DialogInfo> result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});

        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mId.mIndex, 0x10u);
        EXPECT_EQ(result[0].mId.mContentFile, 2);
        EXPECT_EQ(result[0].mTopic.mIndex, 0x123u);
        EXPECT_EQ(result[0].mTopic.mContentFile, 2);
        EXPECT_EQ(result[0].mQuest.mContentFile, 2);
        ASSERT_EQ(result[0].mChoices.size(), 2u);
        EXPECT_EQ(result[0].mChoices[0].mContentFile, 2);
        EXPECT_TRUE(result[0].mChoices[1].isZeroOrUnset());
    }

    TEST(ESM4DialogInfoTest, readsAResponseOfAnotherGameAsOne)
    {
        // An Oblivion record: TRDT of 16 bytes, CTDA of 24
        std::string trdt;
        append<std::uint32_t>(trdt, 4);
        append<std::int32_t>(trdt, 5);
        append<std::uint32_t>(trdt, 0);
        append<std::uint32_t>(trdt, 3);
        std::string condition = conditionData(0, 1.f, 14, 0).substr(0, 24);
        const std::string data = subRecord("TRDT", trdt) + zString("NAM1", "Hello") + subRecord("CTDA", condition)
            + subRecord("CTDA", condition);

        const std::vector<ESM4::DialogInfo> infos = loadRecords<ESM4::DialogInfo>("INFO", record("INFO", 1, data));

        ASSERT_EQ(infos.size(), 1u);
        ASSERT_EQ(infos[0].mResponses.size(), 1u);
        EXPECT_EQ(infos[0].mResponses[0].mText, "Hello");
        EXPECT_EQ(infos[0].mResponses[0].mEmotionType, 4u);
        EXPECT_EQ(infos[0].mResponses[0].mNumber, 3);
        EXPECT_EQ(infos[0].mConditions.size(), 2u);
        EXPECT_TRUE(infos[0].mTopic.isZeroOrUnset());
    }

    std::string everyTopicSubRecord()
    {
        return zString("EDID", "TopicId") + valueSubRecord<std::uint32_t>("QSTI", 0x00010001)
            + valueSubRecord<std::uint32_t>("INFC", 0x00010002) + valueSubRecord<std::int32_t>("INFX", 3)
            + valueSubRecord<std::uint32_t>("QSTI", 0x00010003) + valueSubRecord<std::uint32_t>("QSTR", 0x00010004)
            + zString("FULL", "Topic name") + valueSubRecord<float>("PNAM", 50.f) + zString("TDUM", "dumb")
            + subRecord("DATA", std::string("\x03\x02", 2));
    }

    TEST(ESM4DialogueFalloutTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Dialogue> topics = loadTopics(fallout("DIAL", 0x20, everyTopicSubRecord()));

        ASSERT_EQ(topics.size(), 1u);
        const ESM4::Dialogue& topic = topics[0];
        EXPECT_EQ(topic.mId.toUint32(), 0x20u);
        EXPECT_EQ(topic.mEditorId, "TopicId");
        EXPECT_EQ(topic.mTopicName, "Topic name");
        ASSERT_EQ(topic.mQuests.size(), 2u);
        EXPECT_EQ(topic.mQuests[1].toUint32(), 0x00010003u);
        ASSERT_EQ(topic.mQuestsRemoved.size(), 1u);
        EXPECT_EQ(topic.mQuestsRemoved[0].toUint32(), 0x00010004u);
        ASSERT_EQ(topic.mSharedInfos.size(), 1u);
        EXPECT_EQ(topic.mSharedInfos[0].mInfo.toUint32(), 0x00010002u);
        EXPECT_EQ(topic.mSharedInfos[0].mIndex, 3);
        EXPECT_EQ(topic.mPriority, 50.f);
        EXPECT_EQ(topic.mTextDumb, "dumb");
        EXPECT_EQ(topic.mDialType, 3);
        EXPECT_EQ(topic.mDialFlags, ESM4::Dialogue::Flag_TopLevel);
    }

    TEST(ESM4DialogueFalloutTest, leavesAnUnusedPairOfAnotherSizeAlone)
    {
        const std::vector<ESM4::Dialogue> topics = loadTopics(fallout("DIAL", 0x20,
            zString("EDID", "T") + subRecord("INFC", "") + subRecord("INFX", std::string(2, 'x'))
                + subRecord("DATA", std::string("\x00\x01", 2))));

        ASSERT_EQ(topics.size(), 1u);
        EXPECT_TRUE(topics[0].mSharedInfos.empty());
        EXPECT_EQ(topics[0].mDialFlags, ESM4::Dialogue::Flag_Rumors);
    }

    TEST(ESM4DialogueFalloutTest, readsDataOfOneByteAsTheTypeOnly)
    {
        const std::vector<ESM4::Dialogue> topics = loadTopics(fallout("DIAL", 0x20, subRecord("DATA", "\x07")));

        ASSERT_EQ(topics.size(), 1u);
        EXPECT_EQ(topics[0].mDialType, 7);
        EXPECT_EQ(topics[0].mDialFlags, 0);
    }

    TEST(ESM4DialogueFalloutTest, rejectsWhatNoGameWrites)
    {
        EXPECT_EQ(
            topicFailure(subRecord("DATA", std::string(3, 'x'))), "ESM4::DIAL::load - DATA has an unexpected size");
        EXPECT_EQ(
            topicFailure(subRecord("PNAM", std::string(3, 'x'))), "ESM4::DIAL::load - PNAM has an unexpected size");
        EXPECT_EQ(topicFailure(subRecord("ZZZZ", "1")), "ESM4::DIAL::load - Unknown subrecord ZZZZ");
        EXPECT_EQ(topicFailure(zString("EDID", "x") + std::string(3, '\0')),
            "ESM4::DIAL::load - record has unread bytes after its last sub-record");
    }

    ESM4::DialogueCensus census(const std::string& plugin)
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        reader.setModIndex(0);
        ESM4::DialogueCensus result;
        result.collect(reader);
        EXPECT_THAT(result.getFatalErrors(), IsEmpty());
        return result;
    }

    /// SCHR of 20 bytes for a script of the given size, then its code
    std::string resultScript(std::uint32_t size)
    {
        std::string header;
        append<std::uint32_t>(header, 0);
        append<std::uint32_t>(header, 0);
        append<std::uint32_t>(header, size);
        append<std::uint32_t>(header, 0);
        append<std::uint16_t>(header, 0);
        append<std::uint16_t>(header, 1);
        return subRecord("SCHR", header) + subRecord("SCDA", std::string(size, '\x1d'));
    }

    TEST(ESM4DialogueCensusTest, countsWhatTopicsAndResponsesHave)
    {
        const std::string topicA = fallout("DIAL", 0x100,
            zString("EDID", "TopicA") + valueSubRecord<std::uint32_t>("QSTI", 0x500) + zString("FULL", "Topic A")
                + valueSubRecord<float>("PNAM", 5.5f) + subRecord("DATA", std::string("\x00\x02", 2)));
        const std::string topicB = fallout("DIAL", 0x101, zString("EDID", "TopicB") + subRecord("DATA", "\x01"));
        const std::string first = fallout("INFO", 0x200,
            infoData(0, 0, 0x0004) + valueSubRecord<std::uint32_t>("QSTI", 0x500) + responseData(0, 0, 1, 0, 0)
                + zString("NAM1", "Hello") + subRecord("CTDA", conditionData(0x40, 1.f, 14, 0))
                + subRecord("CTDA", conditionData(0x00, 2.f, 72, 0x700)) + valueSubRecord<std::uint32_t>("TCLT", 0x101)
                + resultScript(4) + subRecord("NEXT", ""));
        const std::string second = fallout("INFO", 0x201,
            infoData(1, 2, 0x0002) + valueSubRecord<std::uint32_t>("QSTI", 0x500)
                + valueSubRecord<std::uint32_t>("PNAM", 0x200) + responseData(3, 1, 1, 0x800, 0)
                + zString("NAM1", "One") + responseData(3, 1, 2, 0, 0) + zString("NAM1", "Two")
                + valueSubRecord<std::uint32_t>("NAME", 0x101) + zString("RNAM", "Prompt")
                + valueSubRecord<std::uint32_t>("ANAM", 0x900));
        const std::string stray = fallout("INFO", 0x202,
            infoData(0, 0, 0) + valueSubRecord<std::uint32_t>("QSTI", 0x5ff)
                + valueSubRecord<std::uint32_t>("PNAM", 0x300) + valueSubRecord<std::uint32_t>("TCLT", 0x999));
        const std::string plugin = falloutHeader() + falloutTopGroup("QUST", fallout("QUST", 0x500, ""))
            + falloutTopGroup("DIAL", topicA + topicGroup(0x100, first + second) + topicB + topicGroup(0x999, stray));

        const ESM4::DialogueCensus counted = census(plugin);
        const ESM4::DialogueCensus::Summary summary = counted.summarize();

        EXPECT_EQ(summary.mTopics, 2u);
        EXPECT_THAT(summary.mTopicTypes, ElementsAre(Pair(0, 1u), Pair(1, 1u)));
        EXPECT_EQ(summary.mTopicsTopLevel, 1u);
        EXPECT_THAT(summary.mTopLevelTypes, ElementsAre(Pair(0, 1u)));
        EXPECT_EQ(summary.mTopicsNoQuest, 1u);
        EXPECT_EQ(summary.mTopicsNamed, 1u);
        EXPECT_EQ(summary.mTopicsNoResponses, 1u);
        EXPECT_THAT(summary.mPriorities, ElementsAre(Pair(5, 1u)));

        EXPECT_EQ(summary.mInfos, 3u);
        EXPECT_EQ(summary.mTopicNotRead, 1u);
        EXPECT_EQ(summary.mQuestNotRead, 1u);
        EXPECT_EQ(summary.mPreviousSet, 2u);
        EXPECT_EQ(summary.mPreviousNotRead, 1u);
        EXPECT_EQ(summary.mChoicesNotTopics, 1u);
        EXPECT_EQ(summary.mWithAddTopics, 1u);
        EXPECT_EQ(summary.mAddTopicsNotTopics, 0u);
        EXPECT_EQ(summary.mWithPrompt, 1u);
        EXPECT_EQ(summary.mWithSpeaker, 1u);
        EXPECT_EQ(summary.mWithBeginScript, 1u);
        EXPECT_EQ(summary.mWithEndScript, 0u);
        EXPECT_EQ(summary.mLines, 3u);
        EXPECT_EQ(summary.mLinesWithSound, 1u);
        EXPECT_EQ(summary.mResponses[0], 1u);
        EXPECT_EQ(summary.mResponses[1], 1u);
        EXPECT_EQ(summary.mResponses[2], 1u);
        EXPECT_EQ(summary.mConditions[0], 2u);
        EXPECT_EQ(summary.mConditions[2], 1u);
        EXPECT_EQ(summary.mFlags[2], 1u); // SayOnce
        EXPECT_EQ(summary.mFlags[1], 1u); // Random
        EXPECT_THAT(summary.mInfoTypes, ElementsAre(Pair(0, 2u), Pair(1, 1u)));
        EXPECT_THAT(summary.mNextSpeakers, ElementsAre(Pair(0, 2u), Pair(2, 1u)));
        ASSERT_EQ(summary.mFunctions.size(), 2u);
        EXPECT_EQ(summary.mFunctions.at(14).mUses, 1u);
        EXPECT_EQ(summary.mFunctions.at(72).mUses, 1u);
        EXPECT_EQ(summary.mFunctions.at(72).mOnTarget, 1u); // the run on of the condition data is the player

        EXPECT_EQ(counted.getUnreadableCount(), 0u);
    }

    TEST(ESM4DialogueCensusTest, writesTheNamesOfTheFunctions)
    {
        const std::string info = fallout("INFO", 0x200,
            infoData(0, 0, 0) + valueSubRecord<std::uint32_t>("QSTI", 0x500)
                + subRecord("CTDA", conditionData(0x00, 1.f, 14, 0)));
        const std::string plugin = falloutHeader()
            + falloutTopGroup(
                "DIAL", fallout("DIAL", 0x100, subRecord("DATA", std::string(2, '\0'))) + topicGroup(0x100, info));

        ESM4::DialogueCensus counted = census(plugin);
        counted.setFunctionNames([](std::uint32_t function) { return function == 14 ? "GetQuestVariable" : ""; });
        std::ostringstream out;
        counted.write(out);

        EXPECT_THAT(out.str(), HasSubstr("Topic records (DIAL): 1"));
        EXPECT_THAT(out.str(), HasSubstr("Response records (INFO): 1"));
        EXPECT_THAT(out.str(), HasSubstr("GetQuestVariable"));
        EXPECT_THAT(out.str(), Not(HasSubstr("could not read")));
    }

    TEST(ESM4DialogueCensusTest, countsARecordTheLoaderRejectsAsUnreadableAndSkipsDeletedOnes)
    {
        const std::string good = fallout("INFO", 0x200, infoData(0, 0, 0));
        const std::string bad = fallout("INFO", 0x201, subRecord("QSTI", std::string(3, 'x')));
        const std::string gone = versionedRecord("INFO", 0x202, "", 15);
        std::string deleted = gone;
        deleted.replace(8, 4, std::string("\x20\0\0\0", 4)); // the flags of the header: deleted
        const std::string plugin = falloutHeader()
            + falloutTopGroup("DIAL",
                fallout("DIAL", 0x100, subRecord("DATA", std::string(2, '\0')))
                    + topicGroup(0x100, good + bad + deleted));

        const ESM4::DialogueCensus counted = census(plugin);

        EXPECT_EQ(counted.summarize().mInfos, 1u);
        EXPECT_EQ(counted.getUnreadableCount(), 1u);
        ASSERT_EQ(counted.getReadErrors().size(), 1u);
        EXPECT_THAT(counted.getReadErrors()[0], StartsWith("INFO: ESM4::INFO::load - QSTI has an unexpected size"));
    }
}
