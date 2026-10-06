#include "placeholderrecords.hpp"

#include <array>
#include <string_view>

#include <components/debug/debuglog.hpp>
#include <components/esm/attr.hpp>
#include <components/esm3/defaultgmsts.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>

#include "esmstore.hpp"

namespace OFWorld
{
    namespace
    {
        const ESM::RefId sPlayerId = ESM::RefId::stringRefId("Player");
        const ESM::RefId sRaceId = ESM::RefId::stringRefId("OpenFallout Placeholder Race");
        const ESM::RefId sClassId = ESM::RefId::stringRefId("OpenFallout Placeholder Class");

        constexpr int sAttributeValue = 50;
        constexpr int sSkillValue = 5;

        struct GlobalDefault
        {
            std::string_view mName;
            ESM::Variant mValue;
        };

        // The globals that the date and time, the new game and the Morrowind crime scripts of the engine read. The
        // clock starts on the first day at nine o'clock; the year is a placeholder as well.
        const std::array<GlobalDefault, 15> sGlobals = { {
            { "gamehour", ESM::Variant(9.f) },
            { "timescale", ESM::Variant(30.f) },
            { "dayspassed", ESM::Variant(1) },
            { "day", ESM::Variant(1) },
            { "month", ESM::Variant(0) },
            { "year", ESM::Variant(1) },
            { "chargenstate", ESM::Variant(0) },
            { "pchascrimegold", ESM::Variant(0) },
            { "pchasgolddiscount", ESM::Variant(0) },
            { "crimegolddiscount", ESM::Variant(0) },
            { "crimegoldturnin", ESM::Variant(0) },
            { "pchasturnin", ESM::Variant(0) },
            { "pcknownwerewolf", ESM::Variant(0) },
            { "werewolfclawmult", ESM::Variant(25.f) },
            { "pcrace", ESM::Variant(0) },
        } };

        template <class T>
        void insertIfMissing(ESMStore& store, const T& record)
        {
            if (store.get<T>().search(record.mId) == nullptr)
                store.insertStatic(record);
        }

        void insertGameSettings(ESMStore& store)
        {
            const auto insert = [&](const char* name, auto&& fill) {
                ESM::GameSetting gmst;
                gmst.mId = ESM::RefId::stringRefId(name);
                gmst.mRecordFlags = 0;
                fill(gmst.mValue);
                insertIfMissing(store, gmst);
            };
            for (std::size_t i = 0; i < ESM::DefaultGmsts::FloatCount; ++i)
                insert(ESM::DefaultGmsts::Floats[i], [&](ESM::Variant& value) {
                    value.setType(ESM::VT_Float);
                    value.setFloat(ESM::DefaultGmsts::FloatsDefaultValues[i]);
                });
            for (std::size_t i = 0; i < ESM::DefaultGmsts::IntCount; ++i)
                insert(ESM::DefaultGmsts::Ints[i], [&](ESM::Variant& value) {
                    value.setType(ESM::VT_Int);
                    value.setInteger(ESM::DefaultGmsts::IntsDefaultValues[i]);
                });
            for (std::size_t i = 0; i < ESM::DefaultGmsts::StringCount; ++i)
                insert(ESM::DefaultGmsts::Strings[i], [&](ESM::Variant& value) {
                    value.setType(ESM::VT_String);
                    value.setString("");
                });
        }

        void insertSkills(ESMStore& store)
        {
            for (int i = 0; i < ESM::Skill::Length; ++i)
            {
                ESM::Skill skill;
                skill.blank();
                skill.mId = ESM::Skill::indexToRefId(i);
                skill.mData.mAttribute = ESM::Attribute::indexToRefId(i % ESM::Attribute::Length);
                insertIfMissing(store, skill);
            }
        }

        // The Morrowind rules of the engine look an effect up whenever an actor's magic effects are read, so each
        // effect has a record, an inert one: no cost, no flags.
        void insertMagicEffects(ESMStore& store)
        {
            for (int i = 0; i < ESM::MagicEffect::Length; ++i)
            {
                ESM::MagicEffect effect;
                effect.blank();
                effect.mId = ESM::MagicEffect::indexToRefId(i);
                insertIfMissing(store, effect);
            }
        }

        void insertPlayer(ESMStore& store)
        {
            ESM::Race race;
            race.blank();
            race.mId = sRaceId;
            race.mName = "Placeholder";
            race.mData.mFlags = ESM::Race::Playable;
            for (int i = 0; i < ESM::Attribute::Length; ++i)
                race.mData.setAttribute(ESM::Attribute::indexToRefId(i), true, sAttributeValue);
            for (int i = 0; i < ESM::Attribute::Length; ++i)
                race.mData.setAttribute(ESM::Attribute::indexToRefId(i), false, sAttributeValue);
            insertIfMissing(store, race);

            ESM::Class cls;
            cls.blank();
            cls.mId = sClassId;
            cls.mName = "Placeholder";
            cls.mData.mIsPlayable = true;
            cls.mData.mAttribute = { ESM::Attribute::Strength, ESM::Attribute::Endurance };
            for (int i = 0; i < 5; ++i)
            {
                cls.mData.mMajorSkills[i] = ESM::Skill::indexToRefId(i);
                cls.mData.mMinorSkills[i] = ESM::Skill::indexToRefId(i + 5);
            }
            insertIfMissing(store, cls);

            ESM::NPC player;
            player.blank();
            player.mId = sPlayerId;
            player.mName = "Player";
            player.mRace = sRaceId;
            player.mClass = sClassId;
            player.mNpdtType = ESM::NPC::NPC_DEFAULT;
            player.mNpdt.mLevel = 1;
            for (int i = 0; i < ESM::Attribute::Length; ++i)
                player.mNpdt.mAttributes[ESM::Attribute::indexToRefId(i)] = sAttributeValue;
            for (int i = 0; i < ESM::Skill::Length; ++i)
                player.mNpdt.mSkills[ESM::Skill::indexToRefId(i)] = sSkillValue;
            player.mNpdt.mHealth = 100;
            player.mNpdt.mMana = 50;
            player.mNpdt.mFatigue = 100;
            player.mNpdt.mDisposition = 50;
            insertIfMissing(store, player);
        }
    }

    bool lacksPlayerRecord(const ESMStore& store)
    {
        return store.get<ESM::NPC>().search(sPlayerId) == nullptr;
    }

    void insertPlaceholderRecords(ESMStore& store)
    {
        Log(Debug::Info) << "The content files hold no player record, using placeholder records to start the game";

        for (const GlobalDefault& global : sGlobals)
        {
            ESM::Global record;
            record.mId = ESM::RefId::stringRefId(global.mName);
            record.mRecordFlags = 0;
            record.mValue = global.mValue;
            insertIfMissing(store, record);
        }
        insertGameSettings(store);
        insertSkills(store);
        insertMagicEffects(store);
        insertPlayer(store);
    }
}
