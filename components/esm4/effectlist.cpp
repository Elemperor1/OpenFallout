#include "effectlist.hpp"

#include "recordreader.hpp"

namespace ESM4
{
    bool readEffectSubRecord(
        RecordReader& in, std::vector<EffectEntry>& effects, std::vector<TargetCondition>& leadingConditions)
    {
        switch (in.type())
        {
            case ESM::fourCC("EFID"):
                in.formId(effects.emplace_back().mBaseEffect);
                return true;
            case ESM::fourCC("EFIT"):
            {
                // EFID is optional in the format reference, so an EFIT that does not follow an EFID without data of its
                // own is an effect that has no base effect.
                EffectData data;
                in.value(data);
                if (effects.empty() || effects.back().mHasData)
                    effects.emplace_back();
                effects.back().mData = data;
                effects.back().mHasData = true;
                return true;
            }
            case ESM::fourCC("CTDA"):
            {
                TargetCondition condition;
                in.condition(condition);
                (effects.empty() ? leadingConditions : effects.back().mConditions).push_back(condition);
                return true;
            }
            default:
                return false;
        }
    }
}
