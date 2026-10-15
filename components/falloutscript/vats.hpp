#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VATS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VATS_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript
{
    /// V.A.T.S., the targeting that stops the game and spends action points on attacks at parts of the body. The
    /// numbers come from the settings of the files; the formulas that are not known for sure are marked.
    namespace Vats
    {
        using Lookup = std::function<std::optional<double>(std::string_view)>;

        struct Rules
        {
            /// The least chance of a shot that can be shot is 0; the most any shot has (fVATSMaxChance)
            double mMaxChance = 95;
            /// How far a blow of the hands or a melee weapon reaches in VATS (fVATSMeleeMaxDistance)
            double mMeleeMaxDistance = 300;
            /// What the chance is multiplied by against a target that is not seen (fVATSStealthMult)
            double mStealthMult = 0.5;
            /// fVATSHitChanceMult
            double mHitChanceMult = 1;
            /// How much the skill with the weapon counts (fVATSSkillFactor)
            double mSkillFactor = 1;
            /// The action points an attack costs by the kind of weapon (fActionPointsAttackUnarmed and the like)
            double mUnarmed = 22;
            double mOneHand = 15;
            double mTwoHand = 30;
            double mPistol = 20;
            double mRifle = 25;
            double mHandle = 20;
            double mLauncher = 40;
            double mGrenade = 35;
            double mMine = 10;
            /// What an attack in VATS costs more for a blow of the hands or a weapon of the hand
            /// (fActionPointsVATSMeleeMult)
            double mMeleeMult = 1;
            /// The action points that come back in a second outside VATS (fActionPointsRestoreRate); New Vegas has a
            /// share of the most instead (0.06) and a perk can change it
            double mRestoreRate = 4;
            double mRestoreShare = 0;
        };

        Rules rules(const Lookup& setting);

        /// What the weapon says that VATS needs (a WEAP record)
        struct Weapon
        {
            /// The animation type of the record: the hands, a melee weapon, a pistol, a rifle ... a mine
            std::uint32_t mAnimationType = 0;
            /// The weapon's own chance in percent in VATS (0 when the record has none)
            double mBaseChance = 0;
            /// The spread of the shots in degrees
            double mSpread = 0;
            /// The action points the record names in place of the kind of weapon's, negative for none
            double mActionPoints = -1;
            /// How far the weapon reaches
            double mRange = 0;
        };

        /// Whether the weapon strikes with the hands or a weapon of the hand (it reaches no further than a blow)
        bool isMelee(std::uint32_t animationType);

        /// The action points one attack with the weapon costs
        double attackCost(const Rules& rules, const Weapon& weapon);

        /// The chance in percent that an attack at a part hits. `partToHit` is the part's own chance in percent,
        /// `distance` how far the target is, `skill` the skill with the weapon, `hidden` that the target does not see
        /// the attacker. A weapon that cannot reach has none. The formula of shots is an estimate (the more the shots
        /// spread over the distance the less of them land on the part; the games' formula is not known to the end).
        double hitChance(
            const Rules& rules, const Weapon& weapon, double partToHit, double distance, double skill, bool hidden);

        /// The action points that come back in `seconds` outside VATS
        double restored(const Rules& rules, double maxActionPoints, double seconds);
    }
}

#endif
