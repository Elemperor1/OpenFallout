#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADRGDL_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADRGDL_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A ragdoll of Fallout 3 and New Vegas: how a body falls and which bones the game moves by hand.
    struct Ragdoll
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::uint32_t mVersion = 0; // NVER
        std::array<std::uint8_t, 14> mData{}; // DATA
        ESM::FormId mActorBase; // XNAM
        ESM::FormId mBodyPartData; // TNAM
        std::array<std::uint8_t, 60> mFeedbackData{}; // RAFD
        std::vector<std::uint8_t> mFeedbackDynamicBones; // RAFB, bone numbers, not decoded
        std::array<std::uint8_t, 24> mPoseMatching{}; // RAPS
        std::string mDeathPose; // ANAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_RGDL4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADRGDL_H
