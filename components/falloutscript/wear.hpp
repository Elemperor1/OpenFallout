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
        /// The health points a shot (or a swing that reaches) takes off the weapon (fDamageToWeaponValue: 0.1 in the
        /// executable, 0.2 in the data of New Vegas)
        double mWeaponShot = 0.2;
        /// The share of the damage that armor takes when it takes a part of a blow (fDamageToArmorPercentage: 0.5 in the
        /// executable, 0.35 in the data), and the most of a blow that armor can take away, in percent (fMaxArmorRating)
        double mArmorShare = 0.35;
        double mMaxRating = 85;

        /// Fallout 3 wears and rates things in another way (Mac round 12AY R4, `Fallout3.exe`): a weapon loses the damage
        /// of its record times a number by its kind (fDamageToWeaponGunMult, ...EnergyMult, ...LauncherMult,
        /// ...MeleeMult: 0.01 in the executable, 0.03, 0.04, 0.06 and 0.05 in the data) and armor gives
        /// base + (max - base) * condition of its rating (fArmorRatingBase, fArmorRatingMax: 0.35 and 1.0 in the
        /// executable, 0.2 and 1.0 in the data). It has no low-condition message.
        bool mFallout3 = false;
        double mGunMult = 0.01;
        double mEnergyMult = 0.01;
        double mLauncherMult = 0.01;
        double mMeleeMult = 0.01;
        double mArmorBase = 0.35;
        double mArmorMax = 1.0;

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

    /// What the condition of armor makes of its rating in the game of the settings: the curve of New Vegas above, or the
    /// line of Fallout 3 from `mArmorBase` at nothing to `mArmorMax` at the whole
    double armorFactor(const Settings& settings, double condition);

    /// A rating (or threshold) of `value` at `condition`. New Vegas gives its whole number times the factor, to the next
    /// whole number up (0x4BDF90); Fallout 3 rounds the product to the nearest whole one (0x47D400).
    double armorValue(const Settings& settings, double value, double condition);

    /// What a card or a list shows of a whole `value` at `condition`: the same number the piece gives
    double armorShown(const Settings& settings, double value, double condition);

    /// What the effect of an ammunition does to a number: its operation is 0 to add, 1 to multiply, 2 to subtract (the kinds of
    /// effect of the executable are 0 damage, 1 resistance, 2 threshold, 3 spread, 4 condition and 5 fatigue)
    double applyAmmoEffect(double value, std::uint32_t operation, double amount);

    /// The health points that a shot takes off the weapon that fires it, before the effects of its ammunition
    double shotPoints(const Settings& settings);

    /// What Fallout 3 takes off a weapon for a shot (or a swing that reaches): the damage of the record times the number
    /// of its kind of weapon, or the number of the record when it has one of its own (0x582640). `type` is the animation
    /// type of the weapon: 0 to 2 melee, 3, 5 and 6 guns, 4 and 7 energy, 8 and 9 launchers, a thing that is thrown or laid
    /// 1.
    struct WeaponWear
    {
        std::uint32_t mDamage = 0;
        std::uint32_t mType = 0;
        bool mOverridden = false;
        double mOverride = 1.0;
    };
    double shotPoints(const Settings& settings, const WeaponWear& weapon);

    /// The health points that armor loses to a blow of `damage` points that the armor takes `resistance` percent of
    /// (the executable's 0x9B5A30: the share times the damage times what the armor takes, which is cut at the most that
    /// armor can take)
    double armorPoints(const Settings& settings, double damage, double resistance);

    /// The same with the threshold of the armor (New Vegas; Fallout 3 has none, and the first overload is the whole of it):
    /// besides the share of the blow that the resistance took, armor loses the share of the smaller of the threshold and
    /// what is left of the blow after the resistance and the threshold, less `floorShare` of the blow (fMinDamMultiplier)
    /// (FalloutNV.exe 0x9B62CD, Mac round 12BA A). `remaining` is the blow after both, before the floor is applied; the
    /// result can be negative, and then the armor does not wear.
    double armorPoints(const Settings& settings, double damage, double resistance, double threshold, double remaining,
        double floorShare);

    /// What `points` of health are of an item whose most health is `maxHealth`, in tenths of a percent. The part of a tenth
    /// that is left goes in `remainder`, which the caller keeps for the next time (a weapon is worn a fraction of a tenth at
    /// a time).
    int tenthsLost(double points, double maxHealth, double& remainder);

    /// Whether the condition (in tenths of a percent) crossed `lowPercent` going down
    bool crossedLow(int before, int after);
}

#endif
