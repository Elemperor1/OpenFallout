#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADPERK_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADPERK_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

#include "script.hpp"

namespace ESM4
{
    class Reader;

    /// A perk of Fallout 3 and New Vegas: what the player can learn at a level up, and what each of its entries
    /// changes.
    struct Perk
    {
        struct Data
        {
            std::uint8_t mTrait = 0;
            std::uint8_t mMinimumLevel = 0;
            std::uint8_t mRanks = 0;
            std::uint8_t mPlayable = 0;
            std::uint8_t mHidden = 0; // not in the shortest DATA
        };

        // PRKC starts a group of conditions, which says who they are about. Conditions of an entry that has no PRKC are
        // in a group with mHasRunOn false.
        struct ConditionGroup
        {
            bool mHasRunOn = false;
            std::uint8_t mRunOn = 0;
            std::vector<TargetCondition> mConditions;
        };

        struct Entry
        {
            std::uint8_t mType = 0; // PRKE, 0 = quest stage, 1 = ability, 2 = entry point
            std::uint8_t mRank = 0;
            std::uint8_t mPriority = 0;
            // DATA depends on the type of the entry:
            ESM::FormId mQuest; // a quest stage: a QUST
            std::uint8_t mQuestStage = 0; // a quest stage
            ESM::FormId mAbility; // an ability: a SPEL
            std::uint8_t mEntryPoint = 0; // an entry point, which of the points of the format reference
            std::uint8_t mFunction = 0; // an entry point
            std::uint8_t mTabCount = 0; // an entry point, how many tabs of conditions
            std::vector<std::uint8_t> mData; // DATA of an entry of another type, 3, 4 or 8 bytes; not decoded
            std::vector<ConditionGroup> mConditionGroups;
            std::uint8_t mFunctionType = 0; // EPFT, which says what EPFD holds
            ESM::FormId mLeveledItem; // EPFD when EPFT is 3: a LVLI
            // EPFD of the other types: any bytes for 0, a float for 1, two floats for 2, nothing for 4, an actor value
            // and a float for 5; not decoded
            std::vector<std::uint8_t> mFunctionData;
            std::string mFunctionText; // EPF2
            std::array<std::uint8_t, 2> mButtonFlags{}; // EPF3
            ScriptDefinition mScript; // the sub-records of a script
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::string mDescription; // DESC
        std::string mIcon; // ICON
        std::vector<TargetCondition> mConditions; // CTDA before the first entry // CTDA and others
        Data mData; // DATA before the first entry
        std::vector<Entry> mEntries; // PRKE starts an entry, the sub-records up to PRKF belong to it
        ScriptDefinition mScript; // a script that comes before the first entry
        std::string mSmallIcon; // MICO

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_PERK4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADPERK_H
