#ifndef OPENFALLOUT_MECHANICS_SUMMONING_H
#define OPENFALLOUT_MECHANICS_SUMMONING_H

#include <string_view>
#include <utility>

#include <components/esm3/refnum.hpp>

namespace ESM
{
    class RefId;
}
namespace OFWorld
{
    class Ptr;
}

namespace OFMechanics
{
    bool isSummoningEffect(ESM::RefId effectId);

    ESM::RefId getSummonedCreature(ESM::RefId effectId);

    void purgeSummonEffect(const OFWorld::Ptr& summoner, const std::pair<ESM::RefId, ESM::RefNum>& summon);

    ESM::RefNum summonCreature(ESM::RefId effectId, const OFWorld::Ptr& summoner);

    void updateSummons(const OFWorld::Ptr& summoner, bool cleanup);
}

#endif
