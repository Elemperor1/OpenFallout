#ifndef OPENFALLOUT_SPELL_PRIORITY_H
#define OPENFALLOUT_SPELL_PRIORITY_H

namespace ESM
{
    struct Spell;
    struct EffectList;
    struct ENAMstruct;
}

namespace OFWorld
{
    class Ptr;
}

namespace OFMechanics
{
    // RangeTypes using bitflags to allow multiple range types, as can be the case with spells having multiple effects.
    enum RangeTypes
    {
        Self = 0x1,
        Touch = 0x10,
        Target = 0x100
    };

    int getRangeTypes(const ESM::EffectList& effects);

    float rateSpell(
        const ESM::Spell* spell, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy, bool checkMagicka = true);
    float rateMagicItem(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);
    float ratePotion(const OFWorld::Ptr& item, const OFWorld::Ptr& actor);

    /// @note target may be empty
    float rateEffect(const ESM::ENAMstruct& effect, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);
    /// @note target may be empty
    float rateEffects(
        const ESM::EffectList& list, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy, bool useSpellMult = true);

    float vanillaRateSpell(const ESM::Spell* spell, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);
}

#endif
