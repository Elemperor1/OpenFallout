#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOCKPICK_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOCKPICK_H

#include <array>

namespace FalloutScript::Lockpick
{
    /// The rules of the lockpicking screen, as the executables have them (LockPickMenu, FalloutNV.exe 0x78E1C0 and
    /// the functions around it; Fallout3.exe is the same). The lengths are in the units of the menu's meter (a pick at
    /// the middle of the screen's safe area is at half of its width).
    struct Rules
    {
        /// The game setting names of the classes are iLockLevelMax* (see Interaction::lockClass); these are the
        /// numbers of the five classes, very easy to very hard, which are the same in both games and are not in
        /// either game's files (the minimum and maximum of each are equal, so the skill does not change them)
        std::array<double, 5> mSweetSpotLength{ 400, 200, 100, 50, 25 };
        std::array<double, 5> mConcentricLength{ 200, 200, 200, 200, 200 };
        /// The skill that a character needs to try a lock of the class; the sixth number is that of a lock that only
        /// its key opens
        std::array<double, 6> mSkillGate{ 0, 25, 50, 75, 100, 101 };
        /// fLockpickBonusHealth and fPickBreakSecs: a new pick has 1 + skill * bonus of health, and binding takes one
        /// of it in that many seconds
        double mBonusHealth = 0.01;
        double mBreakSeconds = 1.0;
        /// fLockSkillBase: what is added to the skill for the chance to force a lock
        double mForceBase = 10.0;
        /// The cylinder turns this many degrees in a second
        double mTurnDegrees = 90.0;
    };

    /// Where the sweet spot is on the meter, how long it is, and how much beyond it the lock still turns: six bands,
    /// nested about the centre; band 0 has the length of the sweet spot, each next is wider by two fifths of the
    /// concentric length, and the last is the sweet spot plus the whole concentric length on each side
    struct SweetSpot
    {
        double mCenter = 0;
        double mLength = 0;
        double mConcentric = 0;
        /// The half-width of the outermost band: the pick turns the lock a little only within it
        double outer() const { return mLength / 2 + mConcentric; }
        double halfWidth(int band) const { return mLength / 2 + band * mConcentric / 5; }
    };

    /// Whether a character of that skill may try a lock of that class (0 very easy .. 4 very hard, 5 only a key)
    bool mayTry(double skill, int lockClass, const Rules& rules = {});

    /// The sweet spot of a lock of the class on a meter of that width; `random` is from 0 to 1. The spot is kept
    /// inside the meter, with its outermost band.
    SweetSpot sweetSpot(int lockClass, double meterWidth, double random, const Rules& rules = {});

    /// How far the cylinder may turn, in degrees, with the pick at that place: 90 only in the innermost band, then 75,
    /// 60, 45, 30 and 15 in the bands out to the last, and from 15 down to nothing beyond them out to the end of the
    /// meter
    double maxTurn(double pick, const SweetSpot& spot, double meterWidth);

    /// The health of a new pick for a character of that skill (the pick breaks when binding has taken it all)
    double pickHealth(double skill, const Rules& rules = {});

    /// The chance, in per cent, to force a lock of that class (from 0 to 100)
    double forceChance(double skill, int lockClass, const Rules& rules = {});
}

#endif
