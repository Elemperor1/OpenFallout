#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_COMBATSTATE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_COMBATSTATE_H

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    /// Who fights whom, and how characters stand to each other: the part of combat that scripts ask about and set
    /// (StartCombat, StopCombat, IsInCombat, GetCombatTarget, SetAlert, SetEnemy, SetAlly, SetUnconscious,
    /// SetRestrained, SetEssential). The fighting itself (a character that has a target goes for it) is for the AI of
    /// the game. It can be used from any thread.
    class CombatState
    {
    public:
        /// Forgets everything, as for a new game
        void clear();

        /// The actor fights the target from now on, and no one else. A target that is the actor itself, or none, is
        /// no fight.
        void start(FormId actor, FormId target);
        /// The same, unless the actor is unconscious (checked with the lock held, so a character that is put out
        /// meanwhile does not start); true when it started or already fought the target
        bool startUnlessOut(FormId actor, FormId target);
        /// The actor stops fighting
        void stop(FormId actor);
        /// The actor is out of every fight (it died or is down): it stops fighting, and so does everyone who went for
        /// it
        void withdraw(FormId actor);
        /// What the actor fights, 0 when it is at peace
        FormId target(FormId actor) const;
        bool inCombat(FormId actor) const { return target(actor) != 0; }
        /// Whether someone fights the target
        bool isFought(FormId target) const;

        /// The actors that fight the target, in the order of their forms
        std::vector<FormId> attackersOf(FormId target) const;

        /// An actor on alert looks for enemies (SetAlert)
        void setAlert(FormId actor, bool alert);
        bool alert(FormId actor) const;

        /// An actor that is unconscious fights no one. With `wakeAfter` (seconds, more than 0) it gets up by itself
        /// when the time has passed (advance(), then takeWoken()); waking it before cancels that. Making an actor that
        /// is unconscious so again without a time leaves the time it has.
        void setUnconscious(FormId actor, bool unconscious, float wakeAfter = 0);
        bool unconscious(FormId actor) const;
        /// The actors that got up by themselves since this was last called
        std::vector<FormId> takeWoken();
        void setRestrained(FormId actor, bool restrained);
        bool restrained(FormId actor) const;

        /// A script made the actor essential (it does not die) or not; none when no script said anything and the
        /// record of the actor decides
        void setEssential(FormId actor, bool essential);
        std::optional<bool> essential(FormId actor) const;

        /// How one faction stands to another, as SetEnemy and SetAlly say (a friend is closer than an ally)
        enum class Relation : std::uint8_t
        {
            Neutral,
            Ally,
            Enemy,
            Friend,
        };
        /// Sets how the first faction stands to the second; how the second stands to the first is its own say
        void setRelation(FormId first, FormId second, Relation relation);
        /// How the first faction stands to the second: none when no script said anything about it
        std::optional<Relation> relation(FormId first, FormId second) const;

        /// A blow that was struck
        struct Hit
        {
            FormId mAttacker = 0;
            FormId mWeapon = 0;
            double mDamage = 0;
            bool mCritical = false;
            int mLimb = -1; // the part of the body, -1 when not known
        };
        /// A blow was struck at the actor: it is the last one it took, and the actor counts as attacked (GetAttacked)
        /// unless the blow is `tolerated` (a blow of the player that an ally or friend lets pass, see countFriendHit).
        /// The executables keep no time on it: an actor stays attacked for as long as it is in a fight, and is not
        /// any more once its fights are over, once it died or is put out.
        void noteHit(FormId victim, const Hit& hit, bool tolerated = false);
        bool attacked(FormId actor) const;
        /// A blow of the attacker at the victim that its faction does not mind: it counts for GetFriendHit when the
        /// attacker's last counted blow at the victim is longer ago than the spacing, and expires after the life of a
        /// blow (setFriendHitRules); with an allowance of 1000 or more the earlier ones are forgotten first. The time
        /// is that of advance(). The answer is how many blows count now, the number that is weighed against the
        /// allowance of the victim.
        int countFriendHit(FormId victim, FormId attacker, int allowance);
        /// How many blows each attacker struck at the actor that count and have not expired
        std::vector<std::pair<FormId, int>> hitsTaken(FormId victim) const;
        /// fFriendHitTimer (the seconds a counted blow lasts, 10 in the games) and fFriendMinimumLastHitTime (the
        /// seconds between two counted blows of one attacker, 0.5)
        void setFriendHitRules(double lifeSeconds, double spacingSeconds);
        /// An actor that ignores friendly hits (SetIgnoreFriendlyHits) turns on no blow of an ally or friend
        void setIgnoreFriendlyHits(FormId actor, bool ignore);
        bool ignoresFriendlyHits(FormId actor) const;
        /// The last blow the actor took, false when it took none
        bool lastHit(FormId actor, Hit& hit) const;
        void advance(float seconds);

        /// A blow that was struck at a character
        struct HitEvent
        {
            FormId mVictim = 0;
            Hit mHit;
        };
        /// The blows since this was last called (the first 1024 kept): the game raises the OnHit blocks of the
        /// characters from them when it is ready for scripts to run, not in the middle of one.
        std::vector<HitEvent> takeHits();

        /// A fight begins or ends
        struct Change
        {
            FormId mActor = 0;
            FormId mTarget = 0; // 0 when the fight ended
            FormId mWas = 0; // what the actor fought before
        };
        /// The fights that began or ended since this was last called (the first 1024 kept)
        std::vector<Change> takeChanges();

    private:
        void noteChange(FormId actor, FormId target, FormId was);
        /// Whether the actor fights or is fought (the caller holds the lock)
        bool inFight(FormId actor) const;
        /// An actor that left its last fight is not attacked any more (the caller holds the lock)
        void settleAttacked(FormId actor);

        mutable std::mutex mMutex;
        std::map<FormId, FormId> mTargets;
        std::set<FormId> mAlert;
        std::set<FormId> mUnconscious;
        std::map<FormId, float> mWakeTimers;
        std::vector<FormId> mWoken;
        std::set<FormId> mRestrained;
        std::map<FormId, bool> mEssential;
        std::map<std::pair<FormId, FormId>, Relation> mRelations;
        std::set<FormId> mAttacked;
        std::set<FormId> mIgnoreFriendlyHits;
        /// The times (on the clock of advance()) of the counted blows, by victim and attacker
        std::map<FormId, std::map<FormId, std::vector<double>>> mHitTimes;
        double mClock = 0;
        double mHitLife = 10;
        double mHitSpacing = 0.5;
        std::map<FormId, Hit> mLastHits;
        std::vector<HitEvent> mHitEvents;
        std::vector<Change> mChanges;
    };
}

#endif
