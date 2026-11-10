#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HARDCORE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HARDCORE_H

#include <array>
#include <functional>
#include <mutex>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    class ActorValues;
    class Magic;

    /// The needs of the hardcore mode of New Vegas: dehydration, hunger and sleep deprivation. Each is a level of the
    /// player (an actor value that goes up as game time passes and down when the player drinks, eats or sleeps) and
    /// each stage of it, from a threshold on, puts a spell on the player (the stage spells of the DEHY, HUNG and SLPD
    /// records); the last stage is the one that kills.
    ///
    /// The rates are the executables'. Time is counted in game minutes: a need goes up by one point each time that
    /// rate x time scale / 60 minutes have gone by (a rate is an interval: a larger one is slower), by as many points
    /// as whole intervals have passed, and the rest of the time is dropped. While the player sleeps no sleep
    /// deprivation is added. It can be used from any thread.
    class HardcoreNeeds
    {
    public:
        enum Need : int
        {
            Dehydration = 0,
            Hunger = 1,
            Sleep = 2,
        };
        static constexpr int needCount = 3;
        /// The actor values of the needs (Dehydration, Hunger and SleepDeprevation of the executables)
        static constexpr int firstActorValue = 73;
        static int actorValueOf(int need) { return firstActorValue + need; }

        struct Stage
        {
            /// The level of the need from which the stage is in force
            int mThreshold = 0;
            /// The spell it puts on the player
            FormId mSpell = 0;
        };
        /// The stages of a need (the records say them); they are put in order of the threshold here
        using StageFinder = std::function<std::vector<Stage>(int need)>;

        HardcoreNeeds(StageFinder stages, ActorValues& values, Magic& magic);

        /// The intervals of the settings (fHCDehydrationRate, fHCStarvationRate, fHCSleepDeprivationRate)
        void setRates(double dehydration, double hunger, double sleep);
        /// Told after the stage spells of the character were changed, which a fatal stage uses to take its health
        void setStageListener(std::function<void(FormId actor)> listener);

        /// Lets the needs of the player go on up to the game minute (counted from any start): the first call, or one
        /// for a time that went backwards (a game that was loaded), only sets where counting starts from
        void update(FormId player, double minutes, double timeScale, bool sleeping);
        /// An hour of sleep goes by: the sleep deprivation gets lower by the amount, or by what there is of it
        void sleepHour(FormId player, double amount);
        /// The hardcore mode is over: the stages come off the player and counting starts over
        void end(FormId player);
        /// Forgets where counting started from, as for a new game or a game that is loaded
        void clear();

    private:
        /// Puts the spell of the stage the level is in on the player and takes those of the other stages off
        void chooseStage(FormId player, int need);

        StageFinder mStages;
        ActorValues& mValues;
        Magic& mMagic;
        mutable std::mutex mMutex;
        std::array<double, needCount> mRates{ 10.0, 25.0, 50.0 };
        /// The game minute each need was last counted up to, 0 for none yet
        std::array<double, needCount> mStamps{};
        /// The level that the stage was last chosen for, none yet below 0
        std::array<double, needCount> mChosen{ -1.0, -1.0, -1.0 };
        std::function<void(FormId)> mListener;
    };
}

#endif
