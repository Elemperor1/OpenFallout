#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LIMBS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LIMBS_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    class ActorValues;

    /// A part of the body that a blow can hit (a part of a BPTD record): the head, an arm, a leg, the torso
    struct BodyPart
    {
        std::string mName;
        /// What VATS aims at, empty for a part it can not aim at
        std::string mTarget;
        /// The actor value that is the condition of the part, -1 for none
        int mCondition = -1;
        /// What a blow at the part does compared to a blow at the body
        double mDamageMult = 1;
        /// The chance in percent that is the part's own in VATS
        int mToHit = 0;
        /// The share in percent of the health of the character that the part stands for
        int mHealthPercent = 100;
    };

    class LimbRecords
    {
    public:
        virtual ~LimbRecords() = default;

        /// The parts of the body of the character or creature, in the order of its record. False when the host can not
        /// tell what the actor is (it is asked again), true with none for an actor that has no parts.
        virtual bool bodyParts(FormId actor, std::vector<BodyPart>& parts) = 0;
    };

    /// The parts of the body that are hit: how much more or less a blow at a part does, and how a part's condition goes
    /// down until it is crippled. It can be used from any thread.
    class Limbs
    {
    public:
        Limbs(LimbRecords& records, ActorValues& values);

        /// The parts of the body of the actor (none when the records tell none)
        std::vector<BodyPart> partsOf(FormId actor);

        /// What a blow at the part does compared to a blow at the body; 1 for a part the actor does not have (-1)
        double damageMultiplier(FormId actor, int part);

        struct Hurt
        {
            /// The condition of the part afterwards in percent, 100 for a part with none
            double mCondition = 100;
            /// This blow crippled the part
            bool mCrippled = false;
        };

        /// A blow of that much damage hits the part: its condition goes down by the share of the health of the part
        /// that the damage is, and the part is crippled when nothing is left
        Hurt hurt(FormId actor, int part, double damage);

        /// The condition of the part in percent
        double condition(FormId actor, int part);

        void clear();

    private:
        const std::vector<BodyPart>* cached(FormId actor);

        LimbRecords& mRecords;
        ActorValues& mValues;
        std::mutex mMutex;
        std::map<FormId, std::vector<BodyPart>> mParts;
    };
}

#endif
