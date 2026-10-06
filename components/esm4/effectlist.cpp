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
                // The size is checked before the position, so that a size no game uses is reported as such.
                EffectData data;
                in.value(data);
                if (effects.empty())
                    in.fail("EFIT comes before EFID");
                effects.back().mData = data;
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
