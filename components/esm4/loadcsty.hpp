#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCSTY_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCSTY_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A combat style of Fallout 3 and New Vegas, which tells an actor how to fight. The three blocks of numbers are
    /// kept as they are.
    struct CombatStyle
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::array<std::uint8_t, 92> mStandard{}; // CSTD, dodge, melee and ranged tendencies
        std::array<std::uint8_t, 84> mAdvanced{}; // CSAD
        std::array<std::uint8_t, 64> mSimple{}; // CSSD

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CSTY4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCSTY_H
