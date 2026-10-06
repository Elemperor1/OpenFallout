#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADSPEL_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADSPEL_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

#include "effectlist.hpp"

namespace ESM4
{
    class Reader;

    /// A spell of Fallout 3 and New Vegas, which is also what a food or a chem item does to the actor that takes it.
    struct Spell
    {
#pragma pack(push, 1)
        struct Data
        {
            std::uint32_t mType = 0;
            std::uint32_t mCost = 0;
            std::uint32_t mLevel = 0;
            std::uint8_t mSpellFlags = 0;
            std::uint8_t mUnused1 = 0;
            std::uint8_t mUnused2 = 0;
            std::uint8_t mUnused3 = 0;
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        Data mData; // SPIT
        std::vector<EffectEntry> mEffects; // EFID/EFIT/CTDA, the effects, each with its conditions
        std::vector<TargetCondition> mConditions; // CTDA before the first effect

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_SPEL4;
    };

    static_assert(sizeof(Spell::Data) == 16);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADSPEL_H
