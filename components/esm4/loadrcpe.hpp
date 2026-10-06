#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADRCPE_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADRCPE_H

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

    /// A recipe of New Vegas: what the player needs to make something and what comes out.
    struct Recipe
    {
#pragma pack(push, 1)
        struct Data
        {
            std::int32_t mSkill = 0; // an actor value
            std::uint32_t mLevel = 0;
            ESM::FormId32 mCategory = 0; // an RCCT
            ESM::FormId32 mSubCategory = 0; // an RCCT
        };
#pragma pack(pop)

        struct Item
        {
            ESM::FormId mItem;
            std::uint32_t mCount = 0;
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::vector<TargetCondition> mConditions; // CTDA
        Data mData; // DATA
        std::vector<Item>
            mIngredients; // RCIL/RCOD/RCQY, RCIL and RCOD each start an item, the RCQY after it says how many
        std::vector<Item> mOutputs;

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_RCPE4;
    };

    static_assert(sizeof(Recipe::Data) == 16);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADRCPE_H
