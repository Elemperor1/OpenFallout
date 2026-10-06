#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADIMAD_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADIMAD_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

#include "recordreader.hpp"

namespace ESM4
{
    class Reader;

    /// An image space modifier of Fallout 3 and New Vegas: an effect on the screen that changes over time, like the
    /// flash of an explosion. What it does is in a long list of tracks of key frames, which are kept as they are.
    struct ImageSpaceModifier
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::vector<std::uint8_t> mData; // DNAM, the settings of the whole effect, not decoded
        std::vector<RawSubRecord> mTracks; // BNAM and others, the tracks of key frames, in file order
        ESM::FormId mRdsd; // RDSD
        ESM::FormId mRdsi; // RDSI

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_IMAD4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADIMAD_H
