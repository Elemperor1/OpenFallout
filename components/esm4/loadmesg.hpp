#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADMESG_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADMESG_H

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

    /// A message of Fallout 3 and New Vegas: the text of a message box or of a pop-up, with its buttons.
    struct Message
    {
        struct Button
        {
            std::string mText;
            std::vector<TargetCondition> mConditions;
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mDescription; // DESC
        std::string mFullName; // FULL
        ESM::FormId mIcon; // INAM, a MICN
        std::vector<RawSubRecord> mNams; // NAM0 and others, one byte each, in file order; not known
        std::uint32_t mMessageFlags = 0; // DNAM
        std::uint32_t mDisplayTime = 0; // TNAM
        std::vector<Button> mButtons; // ITXT/CTDA, ITXT starts a button, the CTDA after it belong to it
        std::vector<TargetCondition> mConditions; // CTDA before the first button

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_MESG4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADMESG_H
