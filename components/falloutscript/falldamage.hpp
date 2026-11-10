#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FALLDAMAGE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FALLDAMAGE_H

#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript::FallDamage
{
    /// The settings that the rule of a fall is worked out from (M13 executable reads: the landing of FalloutNV.exe
    /// 0x8A62B0 and of Fallout3.exe 0x709320). A setting that the files do not have keeps the default of the
    /// executables, which are the values here: the plugins of both games override only the minimum height (600) and
    /// the exponent (1.65), and not the multiplier or the leg settings.
    struct Settings
    {
        double mMinHeight = 256; // fJumpFallHeightMin: a fall of no more than this does no damage
        double mExponent = 2; // fJumpFallHeightExponent
        double mMultiplier = 0.025; // fJumpFallHeightMult
        int mLegChance = 50; // iFallLegDamageChance: the chance in percent for each leg
        double mLegMultiplier = 0.5; // fFallLegDamageMult: the share of the damage that a leg's condition loses
        double mHardLanding = 500; // fHardLandingDamageThreshold (an INI setting of the audio): the heavy landing sound

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The settings that the lookup (a GMST by its editor id) has
        static Settings fromGameSettings(const Lookup& lookup);
    };

    /// The damage of a fall of that much height lost (the height where the fall began minus the height it ended at,
    /// not the time in the air or the speed): none for a fall of no more than the minimum height, else the multiplier
    /// times the excess to the power of the exponent, with no upper limit.
    double damage(const Settings& settings, double heightLost);

    /// The difficulty of the game, from 0 (very easy) to 4 (very hard)
    constexpr int normalDifficulty = 2;

    /// What the difficulty makes of the damage that the player takes (fDiffMultHPToPCVE to VH: 0.5, 0.75, 1, 1.5, 2)
    double damageToPlayerMultiplier(int difficulty);

    /// What the difficulty makes of the damage that the player does to others (fDiffMultHPByPCVE to VH: 2, 1.5, 1,
    /// 0.75, 0.5). A fall has no attacker, so the choice is only whether the one who falls is the player; this is for
    /// the characters of the world, whose falls are not read yet.
    double damageByPlayerMultiplier(int difficulty);

    /// Whether the damage is a heavy landing, which has its own sound (above the threshold; the light one otherwise)
    bool isHardLanding(const Settings& settings, double damage);

    /// Whether the roll (a whole number of 0 to 99) hurts the leg: strictly less than the chance
    bool legHurt(const Settings& settings, int roll);
}

#endif
