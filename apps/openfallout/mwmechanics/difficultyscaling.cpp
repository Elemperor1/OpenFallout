#include "difficultyscaling.hpp"

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

#include "actorutil.hpp"

float scaleDamage(float damage, const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim)
{
    const OFWorld::Ptr& player = OFMechanics::getPlayer();

    static const float fDifficultyMult
        = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>().find("fDifficultyMult")->mValue.getFloat();

    const float difficultyTerm = 0.01f * Settings::game().mDifficulty;

    float x = 0;
    if (victim == player)
    {
        if (difficultyTerm > 0)
            x = fDifficultyMult * difficultyTerm;
        else
            x = difficultyTerm / fDifficultyMult;
    }
    else if (attacker == player)
    {
        if (difficultyTerm > 0)
            x = -difficultyTerm / fDifficultyMult;
        else
            x = fDifficultyMult * (-difficultyTerm);
    }

    damage *= 1 + x;
    return damage;
}
