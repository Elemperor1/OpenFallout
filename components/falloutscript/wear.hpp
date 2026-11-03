#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WEAR_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WEAR_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript::Wear
{
    /// What wears a weapon out and what condition does to an item, from the code and the settings of `FalloutNV.exe` (Mac round
    /// 12AW S12). A condition here is a fraction of the most health the item has (1 for a whole one); the copies of an item
    /// keep it in tenths of a percent (`ActorState::fullCondition`).
    struct Settings
    {
        /// The health points a shot takes off the weapon (fDamageToWeaponValue: 0.1 in the executable, 0.2 in the data of New
        /// Vegas), and by how much a shot of V.A.T.S. takes more (fVATSDamageToWeaponMult: 2.0 in the executable, 1.0 in the data)
        double mWeaponShot = 0.2;
        double mVatsMultiplier = 1.0;
        /// The share of the damage that armor takes when it takes a part of a blow (fDamageToArmorPercentage: 0.5 in the
        /// executable, 0.35 in the data), and the most of a blow that armor can take away, in percent (fMaxArmorRating)
        double mArmorShare = 0.35;
        double mMaxRating = 85;

        using Lookup = std::function<std::optional<double>(std::string_view)>;
        static Settings fromGameSettings(const Lookup& lookup);
    };

    /// The percent of its health below which an item tells the player that it is worn (sWeaponLowCond, sArmorLowCond)
    constexpr double lowPercent = 25;

    /// What the condition of a weapon makes of its damage (0x646D00): nothing above three quarters, below that one less
    /// two thirds of what is missing from three quarters (a weapon at a half does 0.8325 of its damage, at nothing 0.4975)
    double weaponDamageFactor(double condition);

    /// What the condition of armor makes of its rating and its damage threshold (0x646D40): nothing above a half, below that
    /// a half and the condition (0.75 at a quarter, a half at nothing)
    double armorFactor(double condition);

    /// What the effect of an ammunition does to a number: its operation is 0 to add, 1 to multiply, 2 to subtract (the kinds of
    /// effect of the executable are 0 damage, 1 resistance, 2 threshold, 3 spread, 4 condition and 5 fatigue)
    double applyAmmoEffect(double value, std::uint32_t operation, double amount);

    /// The health points that a shot takes off the weapon that fires it, before the effects of its ammunition
    double shotPoints(const Settings& settings, bool vats);

    /// The health points that armor loses to a blow of `damage` points that the armor takes `resistance` percent of
    /// (the executable's 0x9B5A30: the share times the damage times what the armor takes, which is cut at the most that
    /// armor can take)
    double armorPoints(const Settings& settings, double damage, double resistance);

    /// What `points` of health are of an item whose most health is `maxHealth`, in tenths of a percent. The part of a tenth
    /// that is left goes in `remainder`, which the caller keeps for the next time (a weapon is worn a fraction of a tenth at
    /// a time).
    int tenthsLost(double points, double maxHealth, double& remainder);

    /// Whether the condition (in tenths of a percent) crossed `lowPercent` going down
    bool crossedLow(int before, int after);
}

#endif
