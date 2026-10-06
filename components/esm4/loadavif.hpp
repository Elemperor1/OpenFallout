#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADAVIF_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADAVIF_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// An actor value of Fallout 3 and New Vegas: a skill, an attribute or another number an actor has, with the text
    /// the game shows for it.
    struct ActorValueInfo
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::string mDescription; // DESC
        std::string mIcon; // ICON
        std::string mSmallIcon; // MICO, New Vegas only
        std::string mShortName; // ANAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_AVIF4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADAVIF_H
