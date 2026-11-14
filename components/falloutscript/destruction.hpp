#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DESTRUCTION_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DESTRUCTION_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "saveaccess.hpp"
#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;
    class ReferenceState;

    /// One stage of what happens to a thing as it takes damage (a DSTD of a record): it comes when the health of the thing
    /// has gone down to the percentage
    struct DestructionStage
    {
        enum Flags : int
        {
            /// The health stops at the stage's percentage: a blow that would take it lower leaves it there
            capDamage = 1,
            /// The thing is taken out of the world (disabled)
            disable = 2,
            /// The thing is marked destroyed (GetDestroyed)
            destroy = 4,
            /// The stage takes no damage that does not come from the thing itself (the exe's flag, not used here)
            ignoreExternalDamage = 8,
        };

        int mHealthPercent = 0;
        /// The number of the damage stage that the model shows from this stage on (the nodes of its model that are named
        /// for the stage); 0 for none
        int mDamageStage = 0;
        int mFlags = 0;
        /// How much damage a second the thing takes from itself from this stage on (it burns, it bleeds); 0 for none
        int mSelfDamage = 0;
        FormId mExplosion = 0;
        FormId mDebris = 0;
        int mDebrisCount = 0;
        /// The model that takes the place of the thing's own from this stage on; empty for none
        std::string mModel;
    };

    /// The DEST of a record: what a thing can take and what its stages do
    struct DestructionData
    {
        int mHealth = 0;
        /// The flags of the header: 1 for a target of V.A.T.S., 2 for a large actor that destroys
        int mFlags = 0;
        std::vector<DestructionStage> mStages;
    };

    /// What the records of the game say about what can be destroyed
    class DestructionRecords
    {
    public:
        virtual ~DestructionRecords() = default;

        /// The destruction data of the record that the reference is of. False when the record has none, or the host can not
        /// tell what the reference is.
        virtual bool destructionData(FormId reference, DestructionData& data) = 0;
        /// The health that a character or creature starts with (its base health), -1 for a reference that is not one
        virtual double destructionBaseHealth(FormId reference) = 0;
        /// The health a character or creature has now (what a blow of a script leaves it, as the executable reads it
        /// from the actor value), none for a reference that is not one
        virtual std::optional<double> destructionActorHealth(FormId /*reference*/) { return std::nullopt; }
    };

    /// What a blow did to a thing that can be destroyed, for the host to show and to do
    struct DestructionOutcome
    {
        /// False when the blow changed nothing: the thing has no data, the blow is not damage, or the health is gone
        bool mApplied = false;
        /// The health afterwards, and the most it holds (the DEST's, the base health of a character)
        double mHealth = -1;
        double mMaxHealth = 0;
        /// The stage as GetDestructionStage tells it before and after the blow
        int mStageBefore = -1;
        int mStageAfter = -1;
        /// The stages the blow went through, in order: each one's explosion and debris come, and what its flags say
        std::vector<int> mCrossed;
        /// The damage stage the model shows afterwards (0 for none)
        int mDamageStage = 0;
        /// The stage whose model takes the place of the thing's own, -1 when the blow brings none
        int mModelStage = -1;
        bool mDisable = false;
        /// The thing is marked destroyed: by a stage with that flag, or because nothing is left of its health
        bool mDestroyed = false;
        /// The damage a second that the thing takes from itself now, 0 for none
        int mSelfDamage = 0;
    };

    /// The rule of destruction, as the executable has it (NV 0x475B20): what the percentage of health is, the stage that goes
    /// with it, and what a blow does as it goes down through the stages. No state.
    namespace DestructionRule
    {
        /// The percentage of the health in the byte of the executable: the share rounded up, kept to its low byte (a
        /// health above the most wraps around); 0 for a thing whose most is 0
        int percent(double health, double maxHealth);

        /// The stage a thing with that health is in (GetDestructionStage): the number of the first stage whose percentage
        /// is below the thing's, the number of stages when none is
        int stage(const DestructionData& data, double health, double maxHealth);

        /// Whether a thing that has `health` left has been taken out of the world by its stages: a stage with the disable
        /// flag was crossed on the way down from its most (what the blows that brought it here did, told from where it is,
        /// so that a loaded game need not keep it). False for a thing that was never damaged (health below 0).
        bool disables(const DestructionData& data, double health, double maxHealth);

        /// A blow of `amount` to a thing with `health` (negative for a thing that was never damaged: it has its most).
        /// `resulting` is the health afterwards for a character, whose health is taken from its actor value and not
        /// from the blow. Not applied when the blow is not above 0 or the health is 0 already. The outcome says what the
        /// blow went through and what it did; the new health is in mHealth.
        DestructionOutcome damage(const DestructionData& data, double maxHealth, double health, double amount,
            std::optional<double> resulting = {});
    }

    /// What can be destroyed: the health that a thing has taken from, the stage it is in, the model it shows and the damage
    /// it takes from itself. The state starts as the records say (no thing has taken any damage) and is kept as blows made it.
    /// It can be used from any thread: a blow changes the state and the destroyed mark of the reference together, under the
    /// lock that a clear (restore) takes, so one of them comes wholly before the other. The listeners are told after, with
    /// the lock free (they call back into the host), so what they cause is ordered by the host: the engine queues it for the
    /// main thread, where a clear's work comes after the work of the blows that came before it.
    class Destruction
    {
        template <class T>
        friend struct SaveAccess;

    public:
        Destruction(DestructionRecords& records, ReferenceState& references);

        /// Called, with the state unlocked, for every blow that was applied
        using Listener = std::function<void(FormId, const DestructionOutcome&)>;
        void setListener(Listener listener);
        /// Called, with the state unlocked, when a reference is put back as its record made it (restore)
        void setRestoreListener(std::function<void(FormId)> listener);

        /// Forgets everything, as for a new game
        void clear();

        /// A blow of `amount` to the reference (DamageObject, a hit, an explosion, what a thing does to itself). `resulting` is
        /// the health of a character after the blow. The health, the stage and the destroyed mark are brought up to date and
        /// the listener told.
        DestructionOutcome damage(FormId reference, double amount, std::optional<double> resulting = {});

        /// The stage of the reference (GetDestructionStage): -1 for a thing that has no data or never took a blow
        int stage(FormId reference);
        /// The health it has, -1 for a thing that never took a blow
        double health(FormId reference);
        /// The stage whose model the reference shows in place of its own, -1 for none
        int modelStage(FormId reference);
        /// The damage stage its model shows, 0 for none
        int damageStage(FormId reference);
        /// What the reference takes from itself each second
        int selfDamage(FormId reference);
        /// Whether the reference has taken damage (it is not as the record made it)
        bool isDamaged(FormId reference);
        /// Whether a stage that it went through took it out of the world (disabled it); told from its health, so it holds
        /// after a load as well
        bool disabledByStage(FormId reference);
        /// Every reference that has taken damage and is not in its first state (for the host to show again after a load)
        std::vector<FormId> damaged();
        /// Whether the reference has destruction data, and how it reads
        bool dataOf(FormId reference, DestructionData& data);

        /// Puts the reference back as its record made it (ClearDestruction): no damage, no destroyed mark, its own model.
        /// False for a reference that was untouched.
        bool restore(FormId reference);

        /// The things that take damage from themselves are hurt for `seconds` of time: each takes its damage a second times
        /// the seconds, as a blow like any other (the executable does it every frame, skipping what is deleted or disabled)
        void update(double seconds);

        /// Forgets the reference (it was deleted)
        void forget(FormId reference);

    private:
        struct State
        {
            double mHealth = -1;
            int mModelStage = -1;
            int mDamageStage = 0;
            int mSelfDamage = 0;
        };

        DestructionRecords& mRecords;
        ReferenceState& mReferences;
        mutable std::mutex mMutex;
        std::map<FormId, State> mStates;
        Listener mListener;
        std::function<void(FormId)> mRestoreListener;
    };

    /// The section of a saved game that holds what has been damaged
    constexpr std::uint32_t destructionSectionVersion = 1;
    void writeDestruction(Destruction& destruction, SaveWriter& out);
    void readDestruction(Destruction& destruction, SaveReader& in, std::uint32_t version);

    /// Gives the commands DamageObject, GetDestructionStage and ClearDestruction to the interpreter. Those that the
    /// command table does not have are left out.
    void addDestructionCommands(Interpreter& interpreter, Destruction& destruction);
}

#endif
