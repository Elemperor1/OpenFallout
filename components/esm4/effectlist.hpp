#ifndef OPENFALLOUT_COMPONENTS_ESM4_EFFECTLIST_H
#define OPENFALLOUT_COMPONENTS_ESM4_EFFECTLIST_H

#include <cstdint>
#include <vector>

#include <components/esm/formid.hpp>

#include "script.hpp"

namespace ESM4
{
    class RecordReader;

#pragma pack(push, 1)
    /// EFIT: how strong an effect is and for how long.
    struct EffectData
    {
        std::uint32_t mMagnitude = 0;
        std::uint32_t mArea = 0;
        std::uint32_t mDuration = 0;
        std::uint32_t mRange = 0; // 0 = self, 1 = touch, 2 = target
        std::int32_t mActorValue = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(EffectData) == 20);

    /// One effect of a spell or an enchantment: EFID names the magic effect, EFIT says how strong it is, and the
    /// CTDA sub-records that follow are the conditions under which it applies.
    struct EffectEntry
    {
        ESM::FormId mBaseEffect; // EFID, an MGEF; null when the effect has no EFID
        EffectData mData; // EFIT
        bool mHasData = false; // whether the effect has an EFIT
        std::vector<TargetCondition> mConditions; // CTDA
    };

    /// Reads the current sub-record if it is EFID, EFIT or CTDA, which spells and enchantments hold in the order EFID,
    /// EFIT, CTDA*, and returns true. Returns false and reads nothing for any other sub-record. A CTDA before the
    /// first effect is added to `leadingConditions`. EFID starts an effect; EFIT fills the effect that has an EFID and
    /// no EFIT yet, and starts one without a base effect otherwise.
    bool readEffectSubRecord(
        RecordReader& in, std::vector<EffectEntry>& effects, std::vector<TargetCondition>& leadingConditions);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_EFFECTLIST_H
