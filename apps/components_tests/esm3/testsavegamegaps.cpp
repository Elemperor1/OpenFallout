#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm/luascripts.hpp>
#include <components/esm/refid.hpp>
#include <components/esm/stringrefid.hpp>
#include <components/esm3/actoridconverter.hpp>
#include <components/esm3/effectlist.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/lua/configuration.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

// The parts of the saved game records and of the identifiers that the other tests of the format do not reach: the
// Lua scripts of a save, the effects that cannot be written, the identifiers of the actors of old saves, and the
// readers of the content files
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        struct Esm3LuaScriptsTest : public Esm3SaveLoadRecordTest
        {
        };

        LuaScripts makeScripts()
        {
            LuaScripts scripts;
            // data that has a zero byte in it and a script that has neither data nor timers
            scripts.mScripts.push_back({ 3, std::string("data\0more", 9),
                { { LuaTimer::Type::GAME_TIME, 12.5, "onTimer", std::string("argument\0x", 10) },
                    { LuaTimer::Type::REAL_TIME, 0.25, "second", "" } } });
            scripts.mScripts.push_back({ 7, "", {} });
            return scripts;
        }

        TEST_P(Esm3LuaScriptsTest, savesTheTimersAndDataOfEachScript)
        {
            // the scripts of the saves up to this one are named by their path, which is not what the writer writes
            if (GetParam() <= MaxLuaScriptPathFormatVersion)
                return;
            const LuaScripts scripts = makeScripts();

            LuaScripts loaded;
            saveAndLoadRecord(scripts, GetParam(), loaded);
            ASSERT_EQ(loaded.mScripts.size(), 2u);
            EXPECT_EQ(loaded.mScripts[0].mScriptId, 3);
            EXPECT_EQ(loaded.mScripts[0].mData, std::string("data\0more", 9));
            ASSERT_EQ(loaded.mScripts[0].mTimers.size(), 2u);
            EXPECT_EQ(loaded.mScripts[0].mTimers[0].mType, LuaTimer::Type::GAME_TIME);
            EXPECT_EQ(loaded.mScripts[0].mTimers[0].mTime, 12.5);
            EXPECT_EQ(loaded.mScripts[0].mTimers[0].mCallbackName, "onTimer");
            EXPECT_EQ(loaded.mScripts[0].mTimers[0].mCallbackArgument, std::string("argument\0x", 10));
            EXPECT_EQ(loaded.mScripts[0].mTimers[1].mType, LuaTimer::Type::REAL_TIME);
            EXPECT_EQ(loaded.mScripts[0].mTimers[1].mCallbackName, "second");
            EXPECT_THAT(loaded.mScripts[0].mTimers[1].mCallbackArgument, IsEmpty());
            EXPECT_EQ(loaded.mScripts[1].mScriptId, 7);
            EXPECT_THAT(loaded.mScripts[1].mData, IsEmpty());
            EXPECT_THAT(loaded.mScripts[1].mTimers, IsEmpty());
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3LuaScriptsTest, ValuesIn(getFormats()),
            [](const auto& info) { return "V" + std::to_string(info.param); });

        TEST(Esm3LuaScriptsTest, findsTheScriptsOfAnOldSaveByTheirPath)
        {
            // the first save to name a script by its path
            ESM::LuaScriptsCfg cfg;
            ESM::LuaScriptCfg first;
            first.mScriptPath = VFS::Path::Normalized("scripts/first.lua");
            first.mFlags = ESM::LuaScriptCfg::sGlobal;
            ESM::LuaScriptCfg second = first;
            second.mScriptPath = VFS::Path::Normalized("scripts/second.lua");
            cfg.mScripts = { first, second };
            LuaUtil::ScriptsConfiguration configuration;
            configuration.init(cfg, false);

            const auto write = [](ESMWriter& writer) {
                writer.writeHNString("LUAS", "scripts/second.lua");
                writer.writeHNString("LUAS", "scripts/unknown.lua");
            };
            LuaScripts found;
            roundTripWith(MaxLuaScriptPathFormatVersion, write, [&](ESMReader& reader) {
                reader.mScriptsConfiguration = &configuration;
                found.load(reader);
            });
            ASSERT_EQ(found.mScripts.size(), 2u);
            EXPECT_EQ(found.mScripts[0].mScriptId, 1);
            EXPECT_EQ(found.mScripts[1].mScriptId, -1);

            // without the scripts of the content files there is nothing to find them in
            LuaScripts lost;
            roundTripWith(MaxLuaScriptPathFormatVersion, write, [&](ESMReader& reader) { lost.load(reader); });
            ASSERT_EQ(lost.mScripts.size(), 2u);
            EXPECT_EQ(lost.mScripts[0].mScriptId, -1);
        }

        TEST(Esm3LuaScriptsTest, rejectsTheFlagsAndTypesOfAScriptThatAreNotWholeNumbers)
        {
            LuaScriptsCfg cfg;
            roundTripWith(
                CurrentSaveGameFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNString("LUAS", "scripts/odd.lua");
                    writer.startSubRecord("LUAF");
                    writer.writeT<std::uint16_t>(1);
                    writer.endRecord("LUAF");
                },
                [&](ESMReader& reader) {
                    EXPECT_THAT(
                        [&] { cfg.load(reader); }, ThrowsMessage<std::runtime_error>(HasSubstr("Incorrect LUAF size")));
                });
        }

        TEST(Esm3LuaScriptsTest, keepsTheFlagsAndAttachmentsOfAConfiguredScript)
        {
            LuaScriptsCfg cfg;
            LuaScriptCfg script;
            script.mScriptPath = VFS::Path::Normalized("scripts/attached.lua");
            script.mFlags = LuaScriptCfg::sCustom | LuaScriptCfg::sMerge;
            script.mTypes = { fourCC("NPC_"), fourCC("CONT") };
            script.mInitializationData = std::string("init\0", 5);
            script.mRecords.push_back({ true, id("a_record"), "" });
            script.mRecords.push_back({ false, id("another"), "data" });
            script.mRefs.push_back({ true, 5, 0, "" });
            script.mRefs.push_back({ false, 6, 2, std::string("ref\0", 4) });
            cfg.mScripts.push_back(script);

            LuaScriptsCfg loaded;
            saveAndLoadRecord(cfg, CurrentContentFormatVersion, loaded);
            ASSERT_EQ(loaded.mScripts.size(), 1u);
            const LuaScriptCfg& result = loaded.mScripts[0];
            EXPECT_EQ(result.mFlags, script.mFlags);
            EXPECT_EQ(result.mTypes, script.mTypes);
            EXPECT_EQ(result.mInitializationData, script.mInitializationData);
            ASSERT_EQ(result.mRecords.size(), 2u);
            EXPECT_TRUE(result.mRecords[0].mAttach);
            EXPECT_EQ(result.mRecords[0].mRecordId, id("a_record"));
            EXPECT_FALSE(result.mRecords[1].mAttach);
            EXPECT_EQ(result.mRecords[1].mInitializationData, "data");
            ASSERT_EQ(result.mRefs.size(), 2u);
            EXPECT_EQ(result.mRefs[1].mRefnumIndex, 6u);
            EXPECT_EQ(result.mRefs[1].mRefnumContentFile, 2);
            EXPECT_EQ(result.mRefs[1].mInitializationData, std::string("ref\0", 4));
        }

        TEST(Esm3ActorIdConverterTest, replacesTheIdsOfActorsWithTheReferencesTheyWereGiven)
        {
            ActorIdConverter converter;
            converter.mMappings[5] = RefNum{ .mIndex = 50, .mContentFile = 0 };
            converter.mMappings[7] = RefNum{ .mIndex = 70, .mContentFile = 1 };

            const RefNum before{ .mIndex = 1, .mContentFile = 2 };
            RefNum known = before;
            RefNum none = before;
            RefNum later = before;
            RefNum never = before;
            converter.convert(known, 5);
            converter.convert(none, -1);
            converter.convert(later, 6);
            converter.convert(never, 8);

            // an actor that is already known is replaced at once, none is no reference, and the others wait
            EXPECT_EQ(known, (RefNum{ .mIndex = 50, .mContentFile = 0 }));
            EXPECT_FALSE(none.isSet());
            EXPECT_EQ(later, before);
            EXPECT_EQ(never, before);

            // until the actors they name are known
            converter.mMappings[6] = RefNum{ .mIndex = 60, .mContentFile = 0 };
            converter.apply();
            EXPECT_EQ(later, (RefNum{ .mIndex = 60, .mContentFile = 0 }));
            EXPECT_FALSE(never.isSet());
        }

        IndexedENAMstruct makeEffect(RefId effect)
        {
            return IndexedENAMstruct{ {
                                          .mEffectID = effect,
                                          .mSkill = {},
                                          .mAttribute = {},
                                          .mRange = 1,
                                          .mArea = 2,
                                          .mDuration = 3,
                                          .mMagnMin = 4,
                                          .mMagnMax = 5,
                                      },
                0 };
        }

        TEST(Esm3EffectListTest, numbersTheEffectsOfAListInOrder)
        {
            std::vector<ENAMstruct> effects{ makeEffect(MagicEffect::RestoreHealth).mData,
                makeEffect(MagicEffect::FireDamage).mData, makeEffect(MagicEffect::Light).mData };
            EffectList list;
            list.mList.push_back(makeEffect(MagicEffect::Chameleon));
            list.populate(effects);
            ASSERT_EQ(list.mList.size(), 3u);
            EXPECT_EQ(list.mList[0].mIndex, 0u);
            EXPECT_EQ(list.mList[2].mIndex, 2u);
            EXPECT_EQ(list.mList[2].mData.mEffectID, MagicEffect::Light);

            list.mList.erase(list.mList.begin());
            list.updateIndexes();
            EXPECT_EQ(list.mList[0].mIndex, 0u);
            EXPECT_EQ(list.mList[1].mIndex, 1u);
            EXPECT_EQ(list.mList[0].mData.mEffectID, MagicEffect::FireDamage);
        }

        TEST(Esm3EffectListTest, comparesEveryFieldOfTwoEffects)
        {
            const IndexedENAMstruct effect = makeEffect(MagicEffect::RestoreHealth);
            EXPECT_FALSE(effect != makeEffect(MagicEffect::RestoreHealth));
            EXPECT_TRUE(effect != makeEffect(MagicEffect::FireDamage));

            const auto differs = [&](auto change) {
                IndexedENAMstruct other = effect;
                change(other.mData);
                return effect != other;
            };
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mArea = 20; }));
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mRange = 20; }));
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mSkill = Skill::indexToRefId(2); }));
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mAttribute = Attribute::indexToRefId(2); }));
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mMagnMin = 20; }));
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mMagnMax = 20; }));
            EXPECT_TRUE(differs([](ENAMstruct& data) { data.mDuration = 20; }));
            EXPECT_FALSE(differs([](ENAMstruct&) {}));
        }

        TEST(Esm3EffectListTest, doesNotWriteAnEffectThatTheFormatCannotName)
        {
            EffectList unknown;
            unknown.mList.push_back(makeEffect(id("NotAnEffect")));
            // a save that writes the number of the effect has none for it
            EXPECT_THAT(
                [&] {
                    roundTripWith(
                        MaxSerializeEffectRefIdFormatVersion, [&](ESMWriter& writer) { unknown.save(writer); },
                        [](ESMReader&) {});
                },
                ThrowsMessage<std::runtime_error>(HasSubstr("Cannot serialize effect")));

            // and one that writes the identifier has nothing to write for an effect without one
            EffectList empty;
            empty.mList.push_back(makeEffect(RefId()));
            EXPECT_THAT(
                [&] {
                    roundTripWith(
                        CurrentSaveGameFormatVersion, [&](ESMWriter& writer) { empty.save(writer); },
                        [](ESMReader&) {});
                },
                ThrowsMessage<std::runtime_error>(HasSubstr("Cannot serialize empty effect")));
        }

        TEST(Esm3RefIdTest, comparesTheStartEndAndContentOfAStringIdentifier)
        {
            const RefId text = id("Hello World");
            EXPECT_TRUE(text.startsWith("hello"));
            EXPECT_FALSE(text.startsWith("world"));
            EXPECT_TRUE(text.endsWith("WORLD"));
            EXPECT_FALSE(text.endsWith("hello"));
            EXPECT_TRUE(text.contains("o w"));
            EXPECT_FALSE(text.contains("xyz"));

            // anything that is not a string has none of them
            for (const RefId& other : { RefId(), RefId::formIdRefId(FormId{ 1, 1 }), RefId::generated(5),
                     RefId::index(REC_NPC_, 3), RefId::esm3ExteriorCell(1, -2) })
            {
                EXPECT_FALSE(other.startsWith("a")) << other;
                EXPECT_FALSE(other.endsWith("a")) << other;
                EXPECT_FALSE(other.contains("a")) << other;
            }
        }

        TEST(Esm3RefIdTest, hasNoTextForAnIdentifierThatIsNotAString)
        {
            EXPECT_EQ(id("Text").getRefIdString(), "Text");
            EXPECT_THAT(RefId().getRefIdString(), IsEmpty());
            EXPECT_THAT([] { RefId::generated(5).getRefIdString(); },
                ThrowsMessage<std::runtime_error>(HasSubstr("RefId is not a string")));
            EXPECT_THAT(
                [] {
                    RefId::formIdRefId(FormId{ 1, 1 }).getRefIdString();
                },
                ThrowsMessage<std::runtime_error>(HasSubstr("RefId is not a string")));
        }

        TEST(Esm3RefIdTest, rejectsSerializedIdentifiersThatAreBroken)
        {
            EXPECT_THAT([] { RefId::deserialize("short"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("Invalid value size to deserialize: 5")));
            EXPECT_THAT([] { RefId::deserializeText("Esm3ExteriorCell:1:"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("not enough size")));
            EXPECT_THAT([] { RefId::deserializeText("Esm3ExteriorCell:123"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("coordinates separator is not found")));
            EXPECT_EQ(RefId::deserializeText("Esm3ExteriorCell:1:-2"), RefId::esm3ExteriorCell(1, -2));
        }

        TEST(Esm3RefIdTest, countsAndPrintsTheStringIdentifiers)
        {
            const StringRefId first("OFTestCountedIdentifier");
            const std::size_t count = StringRefId::totalCount();
            EXPECT_GE(count, 1u);
            const StringRefId again("oftestcountedidentifier");
            EXPECT_EQ(StringRefId::totalCount(), count);
            EXPECT_TRUE(first == again);

            std::ostringstream out;
            out << first;
            EXPECT_THAT(out.str(), HasSubstr("OFTestCountedIdentifier"));
        }

        TEST(Esm3ReadersCacheGapsTest, tellsWhatItDoesNotKnowOfAReader)
        {
            ReadersCache readers(1);
            EXPECT_EQ(readers.getFileSize(4), 0u);
            EXPECT_THAT(
                [&] { readers.getName(4); }, ThrowsMessage<std::logic_error>(HasSubstr("has not been created yet")));

            {
                const ReadersCache::BusyItem reader = readers.get(2);
                // a reader that was asked for but not opened has no name and no size
                EXPECT_THAT([&] { readers.getFileSize(2); },
                    ThrowsMessage<std::logic_error>(HasSubstr("has not been opened yet")));
            }
            // and a closed one that was never opened has nothing to remember
            EXPECT_THAT([&] { readers.getFileSize(2); },
                ThrowsMessage<std::logic_error>(HasSubstr("has forgotten its file size")));
            EXPECT_THAT(
                [&] { readers.getName(2); }, ThrowsMessage<std::logic_error>(HasSubstr("has forgotten its filename")));

            readers.clear();
            EXPECT_EQ(readers.getFileSize(2), 0u);
            EXPECT_THAT(
                [&] { readers.getName(2); }, ThrowsMessage<std::logic_error>(HasSubstr("has not been created yet")));
        }
    }
}
