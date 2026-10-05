#ifndef OPENFALLOUT_MECHANICS_DISEASE_H
#define OPENFALLOUT_MECHANICS_DISEASE_H

#include <components/esm3/loadmgef.hpp>
#include <components/misc/rng.hpp>
#include <components/misc/strings/format.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"
#include "spells.hpp"

namespace OFMechanics
{

    /// Call when \a actor has got in contact with \a carrier (e.g. hit by him, or loots him)
    /// @param actor The actor that will potentially catch diseases. Actors cannot catch diseases from the player.
    /// @param carrier The disease carrier.
    inline void diseaseContact(const OFWorld::Ptr& actor, const OFWorld::Ptr& carrier)
    {
        if (!carrier.getClass().isActor() || carrier == getPlayer())
            return;

        float fDiseaseXferChance = OFBase::Environment::get()
                                       .getESMStore()
                                       ->get<ESM::GameSetting>()
                                       .find("fDiseaseXferChance")
                                       ->mValue.getFloat();

        const MagicEffects& actorEffects = actor.getClass().getCreatureStats(actor).getMagicEffects();

        Spells& spells = carrier.getClass().getCreatureStats(carrier).getSpells();
        for (const ESM::Spell* spell : spells)
        {
            if (actor.getClass().getCreatureStats(actor).getSpells().hasSpell(spell->mId))
                continue;

            float resist = 0.f;
            if (Spells::hasCorprusEffect(spell))
                resist = 1.f
                    - 0.01f
                        * (actorEffects.getOrDefault(ESM::MagicEffect::ResistCorprusDisease).getMagnitude()
                            - actorEffects.getOrDefault(ESM::MagicEffect::WeaknessToCorprusDisease).getMagnitude());
            else if (spell->mData.mType == ESM::Spell::ST_Disease)
                resist = 1.f
                    - 0.01f
                        * (actorEffects.getOrDefault(ESM::MagicEffect::ResistCommonDisease).getMagnitude()
                            - actorEffects.getOrDefault(ESM::MagicEffect::WeaknessToCommonDisease).getMagnitude());
            else if (spell->mData.mType == ESM::Spell::ST_Blight)
                resist = 1.f
                    - 0.01f
                        * (actorEffects.getOrDefault(ESM::MagicEffect::ResistBlightDisease).getMagnitude()
                            - actorEffects.getOrDefault(ESM::MagicEffect::WeaknessToBlightDisease).getMagnitude());
            else
                continue;

            int x = static_cast<int>(fDiseaseXferChance * 100 * resist);
            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
            if (Misc::Rng::rollDice(10000, prng) < x)
            {
                // Contracted disease!
                OFMechanics::CreatureStats& creatureStats = actor.getClass().getCreatureStats(actor);
                creatureStats.getSpells().add(spell);
                creatureStats.getActiveSpells().addSpell(spell, actor, false);
                OFBase::Environment::get().getWorld()->applyLoopingParticles(actor);

                if (actor == getPlayer())
                {
                    std::string msg = OFBase::Environment::get()
                                          .getESMStore()
                                          ->get<ESM::GameSetting>()
                                          .find("sMagicContractDisease")
                                          ->mValue.getString();
                    msg = Misc::StringUtils::format(msg, spell->mName);
                    OFBase::Environment::get().getWindowManager()->messageBox(msg);
                }
            }
        }
    }
}

#endif
