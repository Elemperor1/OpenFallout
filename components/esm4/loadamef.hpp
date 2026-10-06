#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADAMEF_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADAMEF_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// An ammo effect of New Vegas: what a special kind of ammunition does to the weapon or to the target.
    struct AmmoEffect
    {
#pragma pack(push, 1)
        struct Data
        {
            std::uint32_t mType = 0; // damage, resistance, threshold, spread, condition or fatigue
            std::uint32_t mOperation = 0; // add, multiply or subtract
            float mValue = 0.f;
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        Data mData; // DATA

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_AMEF4;
    };

    static_assert(sizeof(AmmoEffect::Data) == 12);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADAMEF_H
