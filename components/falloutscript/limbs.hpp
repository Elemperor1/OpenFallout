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
        /// The type of the part (0 torso, 1 and 2 head, 3 and 4 left arm, 5 and 6 right arm, 7 to 9 left leg, 10 to 12
        /// right leg, 13 brain, 14 weapon), which is what scripts call the hit location: a body has at most one part of
        /// a type, and the games find a part by it, not by its place in the record. -1 when the record does not say.
        int mType = -1;
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

        /// What a blow at the part (of that type) does compared to a blow at the body; 1 for a part the actor does not
        /// have (or -1)
        double damageMultiplier(FormId actor, int type);

        struct Hurt
        {
            /// The condition of the part afterwards in percent, 100 for a part with none
            double mCondition = 100;
            /// This blow crippled the part
            bool mCrippled = false;
        };

        /// The limb damage of a blow that does `damage` to the part (of that type): the damage as a share, in percent,
        /// of the part's share of the base health of the actor (the damage itself for an actor with no health, none for
        /// a part that stands for none of it)
        double limbDamage(FormId actor, int type, double damage);

        /// A blow of that much limb damage (limbDamage() of the armored damage, scaled) hits the part (of that type):
        /// the condition goes down by it, and the part is crippled when nothing is left
        Hurt hurt(FormId actor, int type, double limbDamage);

        /// The condition of the part (of that type) in percent
        double condition(FormId actor, int type);

        void clear();

    private:
        const std::vector<BodyPart>* cached(FormId actor);
        static const BodyPart* byType(const std::vector<BodyPart>* parts, int type);

        LimbRecords& mRecords;
        ActorValues& mValues;
        std::mutex mMutex;
        std::map<FormId, std::vector<BodyPart>> mParts;
    };
}

#endif
