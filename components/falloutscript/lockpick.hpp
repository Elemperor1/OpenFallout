#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOCKPICK_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOCKPICK_H

#include <array>
#include <functional>
#include <string>
#include <vector>

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

    /// What the screen is to do on a step of the game: a sound, or an end of the game that the host carries out
    enum class Event
    {
        /// UILockpickingEnter, the pick has flown in
        Enter,
        /// The player has no pick to begin with, or has used the last (the message sOutOfLockpicks)
        OutOfPicks,
        /// UILockpickingPickMovement
        Move,
        /// UILockpickingCylinderTurn
        Turn,
        /// UILockpickingCylinderSqueakA and B, at 30 and 60 degrees
        SqueakA,
        SqueakB,
        /// UILockpickingCylinderStop, and the loop UILockpickingPickTensionLPM while the pick is under strain
        Stop,
        TensionStart,
        TensionEnd,
        /// UILockpickingPickBreak: a pick is gone (the host takes one from the player)
        Break,
        /// UILockpickingUnlock: the lock is open (the host opens it, with the experience and the karma)
        Unlock,
        /// The forcing failed (UILockpickingForceFail): the lock is broken for good and the screen closes
        ForceFailed,
    };

    /// The game of picking one lock: the sweet spot, the pick on the meter, the cylinder that turns as far as the
    /// pick's place allows, the strain on the pick and its breaking, as LockPickMenu does it (FalloutNV.exe, the update
    /// 0x78EB50 and the functions it calls; round 12AI and 12AJ). The host feeds it the time and what the player does,
    /// plays the sounds of the events and draws the lock and pick by `view()`.
    class Game
    {
    public:
        struct Setup
        {
            /// 0 very easy to 4 very hard (a lock that only its key opens is not picked)
            int mLockClass = 2;
            double mSkill = 0;
            int mPicks = 0;
            /// The width of the meter in the units of the menu
            double mMeterWidth = 1000;
            Rules mRules;
        };

        enum class State
        {
            /// The lock and the pick fly in
            Intro,
            Idle,
            Turning,
            /// The cylinder is at the end of the pick's play and the pick is under strain
            Binding,
            /// The pick has broken and its pieces fly off, then a new one is taken
            Snapping,
            /// The new pick flies in
            Waiting,
            Opened,
            Failed,
        };

        /// `random` gives numbers from 0 up to but not including 1 (the sweet spot, which of the steps of the pick
        /// make a sound)
        Game(const Setup& setup, std::function<double()> random);

        struct Input
        {
            /// Seconds since the last step
            double mSeconds = 0;
            /// How far the mouse moved to the right, in the units of the menu
            double mMove = 0;
            /// How hard the player turns the lock, 0 to 1 (a key that turns it is 1)
            double mTurn = 0;
        };

        /// One step; the events of it in order
        std::vector<Event> step(const Input& input);

        /// The player forces the lock. `roll` is from 0 to 99. A game that is not waiting for a turn does nothing.
        std::vector<Event> force(double roll);

        /// What to draw: the lock and the pick are put at a time of a sequence of their files
        struct View
        {
            /// The place of the pick on the meter, with the shake it has under strain, and the turn of the pick (in
            /// degrees, 90 to the left of the screen to -90 to the right) that goes with it
            double mPick = 0;
            double mPickAngle = 0;
            /// The turn of the cylinder in degrees, 0 to 90, and the sequence of the lock's file that shows it
            double mAngle = 0;
            std::string mLockSequence;
            double mLockTime = 0;
            std::string mPinSequence;
            double mPinTime = 0;
            double mHealth = 1;
        };
        View view() const;

        State state() const { return mState; }
        int picks() const { return mPicks; }
        const SweetSpot& sweetSpot() const { return mSpot; }
        /// How far the lock may turn with the pick where it is
        double limit() const { return mLimit; }
        bool forcing() const { return mForcing; }

        /// The lengths of the sequences of the two files, in seconds
        static constexpr double lockForward = 1.3333;
        static constexpr double lockBackward = 2.9667;
        static constexpr double pinForward = 1.0;
        static constexpr double pinBackward = 0.9667;
        static constexpr double pinLeft = 0.4667;
        /// How long after a break the next pick is taken
        static constexpr double resetSeconds = 1.0;

    private:
        void crossSqueaks(double before, double after, std::vector<Event>& events);

        Setup mSetup;
        std::function<double()> mRandom;
        SweetSpot mSpot;
        State mState = State::Intro;
        double mClock = 0;
        double mPick = 0;
        double mAngle = 0;
        double mLimit = 0;
        double mHealth = 1;
        int mPicks = 0;
        bool mForcing = false;
        bool mMoving = false;
        bool mAnnounced = false;
        double mStepBefore = 0;
        int mShake = 1;
    };
}

#endif
