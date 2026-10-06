#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADLSCR_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADLSCR_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A load screen of Fallout 3 and New Vegas.
    struct LoadScreen
    {
#pragma pack(push, 1)
        struct Location
        {
            ESM::FormId32 mDirect = 0; // a REFR, ACHR or ACRE
            ESM::FormId32 mIndirect = 0; // a WRLD or a CELL
            std::int16_t mGridY = 0;
            std::int16_t mGridX = 0;
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mIcon; // ICON
        std::string mSmallIcon; // MICO, New Vegas only
        std::string mDescription; // DESC
        std::vector<Location> mLocations; // LNAM, where the load screen is used
        ESM::FormId mLoadScreenType; // WMI1, an LSCT, New Vegas only

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_LSCR4;
    };

    static_assert(sizeof(LoadScreen::Location) == 12);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADLSCR_H
