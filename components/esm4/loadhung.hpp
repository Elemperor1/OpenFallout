#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADHUNG_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADHUNG_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A stage of hunger of New Vegas (hardcore mode). It starts at a value and puts an effect on the actor.
    struct HungerStage
    {
#pragma pack(push, 1)
        struct Data
        {
            std::uint32_t mTriggerThreshold = 0;
            ESM::FormId32 mActorEffect = 0; // a SPEL
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        Data mData; // DATA

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_HUNG4;
    };

    static_assert(sizeof(HungerStage::Data) == 8);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADHUNG_H
