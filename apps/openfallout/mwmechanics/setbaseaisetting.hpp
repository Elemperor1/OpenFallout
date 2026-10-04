#ifndef OPENFALLOUT_MWMECHANICS_SETBASEAISETTING_H
#define OPENFALLOUT_MWMECHANICS_SETBASEAISETTING_H

#include <string>

#include "../mwbase/environment.hpp"

#include "../mwworld/esmstore.hpp"

#include "aisetting.hpp"

namespace OFMechanics
{
    template <class T>
    void setBaseAISetting(const ESM::RefId& id, OFMechanics::AiSetting setting, unsigned char value)
    {
        T copy = *OFBase::Environment::get().getESMStore()->get<T>().find(id);
        switch (setting)
        {
            case OFMechanics::AiSetting::Hello:
                copy.mAiData.mHello = value;
                break;
            case OFMechanics::AiSetting::Fight:
                copy.mAiData.mFight = value;
                break;
            case OFMechanics::AiSetting::Flee:
                copy.mAiData.mFlee = value;
                break;
            case OFMechanics::AiSetting::Alarm:
                copy.mAiData.mAlarm = value;
                break;
            default:
                assert(false);
        }
        OFBase::Environment::get().getESMStore()->overrideRecord(copy);
    }
}

#endif
