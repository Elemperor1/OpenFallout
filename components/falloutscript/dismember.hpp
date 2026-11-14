#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DISMEMBER_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DISMEMBER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "limbs.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// The numbers behind the limbs of a body that the executable's rule of dismemberment works with
    namespace DismemberLimb
    {
        /// The type of the part of the body that is the torso (BodyPart::mType)
        constexpr int torso = 0;
        /// A body has parts of the types 0 to 14
        constexpr int count = 15;
        /// The actor value of the condition of the torso (a part with it takes a blow to the body)
        constexpr int torsoCondition = 26;
        /// The conditions of the limbs are the actor values 25 to 31 (the head, the arms, the legs, the brain...)
        constexpr int firstCondition = 25;
        constexpr int lastCondition = 31;
        /// The flags of a part of the body (BPND)
        constexpr int severable = 0x01;
        constexpr int explodable = 0x08;
        constexpr int absoluteChance = 0x40;
    }

    /// What killed: the value that GetCauseofDeath gives. The executable takes it from the kind of thing that dealt
    /// the blow (NV 0x89C0B1-0x89C146 of the function that deals a blow): an explosion 0, the weapon form itself 2, a
    /// projectile 1, anything else (and a blow with no source) 3; a hazard and a poison have 4 and 5.
    namespace DeathCause
    {
        constexpr int none = -1;
        constexpr int explosion = 0;
        constexpr int projectile = 1;
        constexpr int weapon = 2;
        constexpr int other = 3;
    }

    /// The numbers the settings of the games give the rule (the game settings of the same names and the setting of the
    /// executable that limits how many characters lose limbs in a frame)
    struct DismemberSettings
    {
        /// iCombatExplodePartChance: how likely a part that can burst does, in percent, before the part's own chance
        int mExplodePartChance = 75;
        /// iCombatDismemberPartChance
        int mDismemberPartChance = 50;
        /// iMessTargetedLimbExplodeBonus, iMessCrippledLimbExplodeBonus, iMessIntactLimbExplodeBonus: what is added to
        /// the chance of bursting for the limb that was aimed at, for one that is crippled and for one that is not
        int mMessTargetedExplodeBonus = 0;
        int mMessCrippledExplodeBonus = 25;
        int mMessIntactExplodeBonus = 25;
        /// iMessIntactLimbDismemberChance: how likely a limb that is whole comes off when the killer has Bloody Mess
        int mMessIntactDismemberChance = 15;
        /// iMessTorsoExplodeChance: how likely a blow at the torso bursts the body when the killer has Bloody Mess
        int mMessTorsoExplodeChance = 10;
        /// iActorsDismemberedPerFrame (an executable setting, 2): how many characters lose limbs in one frame
        int mPerFrame = 2;
        /// bDisableAllGore: nothing is torn off
        bool mGoreOff = false;

        /// The values from the game settings (`get` gives a setting by name, none when it is not there)
        static DismemberSettings fromGameSettings(const std::function<std::optional<double>(std::string_view)>& get);
    };

    /// What the blow that killed says about itself
    struct DismemberHit
    {
        /// The type of the part that was hit, -1 for a blow at no part
        int mLimb = -1;
        /// The blow bursts what it hits (flag 0x40 of the executable's hit) and the blow tears it off (flag 0x20)
        bool mExplodes = false;
        bool mDismembers = false;
        /// The one that struck has the perk Bloody Mess (the actor value 55 above 0)
        bool mBloodyMess = false;
        /// The weapon's setting for what a hit does to a part (0 as the settings say, 1 tears off only, 2 bursts
        /// only, 3 neither); 0 for a blow with no weapon
        int mOnHit = 0;
        /// The parts that an explosion hit, for a blow of an explosion
        std::vector<int> mExplosionLimbs;
        bool mFromExplosion = false;
    };

    /// One part that the rule has chosen to try
    struct DismemberEntry
    {
        /// The type of the part
        int mLimb = -1;
        /// The actor value of its condition
        int mCondition = -1;
        /// What is added to the chance of tearing off and of bursting
        int mDismemberBonus = 0;
        int mExplodeBonus = 0;
    };

    /// A limb that is torn off
    struct Cut
    {
        int mLimb = -1;
        /// The part burst, and did not come off whole
        bool mExploded = false;
    };

    /// The rule of the executable for the loss of limbs (NV 0x8B43A0 chooses the parts, 0x8B4D10 decides for each, 0x646E50
    /// rolls). Pure.
    namespace DismemberRule
    {
        struct Roll
        {
            bool mExplode = false;
            bool mDismember = false;
        };

        /// The roll for one part (0x646E50): `random` is a number from 0 to 99; the chance of bursting is the part's
        /// own (0 for a part that cannot burst) added to `explodeBonus` when the part's chance is absolute, and else
        /// (explodePartChance + explodeBonus) percent of the part's own; the chance of tearing off is
        /// dismemberPartChance + `dismemberBonus`. A burst comes before a tear. The weapon's `onHit` then changes what
        /// came out, when something did: 1 makes it a tear, 2 a burst, 3 nothing.
        Roll roll(const DismemberSettings& settings, int random, int explodableChance, int dismemberBonus,
            int explodeBonus, bool absolute, int onHit);

        /// The parts that are tried (0x8B43A0). `condition` gives the current value of an actor value of the character
        /// and `random` a number from 0 to 99. With Bloody Mess the part that was hit is tried with the bonus for the
        /// aimed limb, each crippled limb with the one for the crippled and each whole limb that the roll lets with
        /// the one for the whole (a blow at the torso that bursts it brings them all); without it only a hit part
        /// that is crippled is tried, and the parts of an explosion that the host names.
        struct Plan
        {
            std::vector<DismemberEntry> mEntries;
            bool mTorsoExplodes = false;
        };
        Plan plan(const DismemberSettings& settings, const std::vector<BodyPart>& parts, const DismemberHit& hit,
            const std::function<double(int)>& condition, const std::function<int()>& random);

        /// The decision for one part (0x8B4D10): nothing for a part that is not a limb or has already gone, else the
        /// roll (a blow that bursts or tears says so itself and no roll is needed). The cut names the part by the
        /// first part of the body that has the same condition, as the executable's loop does.
        std::optional<Cut> decide(const DismemberSettings& settings, const std::vector<BodyPart>& parts,
            const DismemberHit& hit, const DismemberEntry& entry, bool torsoExplodes,
            const std::function<bool(int)>& isGone, const std::function<int()>& random);

        /// Everything for one blow: the plan and the decision for each part of it
        std::vector<Cut> cuts(const DismemberSettings& settings, const std::vector<BodyPart>& parts,
            const DismemberHit& hit, const std::function<double(int)>& condition,
            const std::function<bool(int)>& isGone, const std::function<int()>& random);

        /// What GetCauseofDeath says for a blow whose source is of that kind (the executable's form types: 0x28 the
        /// weapon, 0x33 a projectile, the explosion by its own test)
        int causeOf(bool fromExplosion, bool fromProjectile, bool fromWeapon);

        /// The whole hours of the game's clock that GetTimeDead counts from (0x867E30 floors the days passed, a number with
        /// a fraction in the executables, times 24): the whole days that the world counts, and the hour of the day it is
        double wholeHours(double daysPassed, double hourOfDay);
    }

    /// Which limbs of each character are gone, what killed it and who. A limb is a type of part (0 to 14). It can be
    /// used from any thread.
    class Dismemberment
    {
        template <class T>
        friend struct SaveAccess;

    public:
        /// Whether the limb of that type is gone from the character
        bool isGone(FormId actor, int limb);
        /// The bit of each limb that is gone (bit n for the limb of type n), and the bits of those that burst
        std::uint16_t mask(FormId actor);
        std::uint16_t exploded(FormId actor);
        /// What killed the character (the value of DeathCause), -1 when nothing was kept
        int cause(FormId actor);
        /// The reference that dealt the blow that killed it, 0 for none
        FormId killer(FormId actor);

        /// Marks the limb as gone (a limb that is out of the 0 to 14 is not)
        void markGone(FormId actor, int limb, bool exploded);
        /// Keeps what killed the character and who
        void recordDeath(FormId actor, int cause, FormId killer);
        /// The critical stage of the character: 0 none, 1 goo starts, 2 goo ends, 3 disintegration starts, 4 it ends
        /// (the enum of the executable at actor + 0x10C: the body of a character that an energy weapon or a goo gun
        /// killed goes through it)
        int criticalStage(FormId actor);
        void setCriticalStage(FormId actor, int stage);
        /// The hour of the game (counted from its start, whole) at which the character died, 0 for none (GetTimeDead)
        double deathHour(FormId actor);
        void setDeathHour(FormId actor, double hour);
        /// A limb that comes back (the character is brought back whole)
        void restore(FormId actor);

        /// A new frame begins: the characters that lose limbs in it are counted again
        void newFrame();
        /// Whether one more character may lose limbs in this frame (it is counted)
        bool takeSlot(int perFrame);

        void clear();
        void forget(FormId actor);
        /// The characters that have anything kept
        std::vector<FormId> actors();

    private:
        struct Entry
        {
            std::uint16_t mMask = 0;
            std::uint16_t mExploded = 0;
            int mCause = DeathCause::none;
            FormId mKiller = 0;
            int mCritical = 0;
            double mDeathHour = 0;
        };

        std::mutex mMutex;
        std::map<FormId, Entry> mEntries;
        int mThisFrame = 0;
    };

    constexpr std::uint32_t dismemberSectionVersion = 1;
    void writeDismemberment(Dismemberment& dismemberment, SaveWriter& out);
    void readDismemberment(Dismemberment& dismemberment, SaveReader& in, std::uint32_t version);

    /// What the commands about limbs need to know of a reference
    class DismemberRecords
    {
    public:
        virtual ~DismemberRecords() = default;
        virtual bool dismemberIsActor(FormId reference) = 0;
        /// A critical stage was set for the actor (the host shows what the stage does)
        virtual void criticalStageSet(FormId /*actor*/, int /*stage*/) {}
    };

    namespace CriticalStage
    {
        constexpr int none = 0;
        constexpr int gooStart = 1;
        constexpr int gooEnd = 2;
        constexpr int disintegrateStart = 3;
        constexpr int disintegrateEnd = 4;
    }

    /// Gives IsLimbGone, GetCauseofDeath, IsInCriticalStage and SetCriticalStage to the interpreter
    void addDismemberCommands(Interpreter& interpreter, Dismemberment& dismemberment, DismemberRecords& records);
}

#endif
