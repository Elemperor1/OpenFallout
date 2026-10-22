#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REPUTATION_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REPUTATION_H

#include <array>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// What the records of the game say about reputations
    class ReputationRecords
    {
    public:
        virtual ~ReputationRecords() = default;

        /// The cap of a reputation (the `DATA` of its `REPU` record, the value at which a side is full) and its name.
        /// False for a form that is no reputation.
        virtual bool reputation(FormId reputation, double& cap, std::string& name) = 0;
        /// The value of a game setting, the fallback when no file has it
        virtual double setting(std::string_view name, double fallback) = 0;
    };

    /// The fame and the infamy that the player has with the factions and towns of New Vegas. Each reputation has two
    /// sides, 1 for the positive one (fame) and 0 for the negative one (infamy), of points that go up to the cap of the
    /// record. The level of a side (0 to 3) is how far it is to the cap: below 15%, 50%, 100% and full. The two levels
    /// together make the class that GetReputationThreshold tells (neutral, idolized, hated, mixed ...). It can be used
    /// from any thread.
    class Reputation
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit Reputation(ReputationRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        /// The points of a side; 0 for a side that is none or a reputation that is none
        double value(FormId reputation, int side);
        /// Puts the points of a side, whatever the cap is. False for a side that is none or a reputation that is none.
        bool set(FormId reputation, int side, double points);

        struct Bump
        {
            /// Whether the reputation was changed: false for a side or a step that is none, or a reputation that is
            /// none
            bool mApplied = false;
            /// The side the step went to
            int mSide = 0;
            /// The name of the reputation, for the message that tells of the change
            std::string mName;
            /// Whether the level of the side changed
            bool mLevelChanged = false;
            /// The levels of the two sides after the change
            int mPositiveLevel = 0;
            int mNegativeLevel = 0;
        };

        /// Raises a side by the amount of a step, from 1 (very minor) to 5 (very major), up to the cap
        Bump add(FormId reputation, int side, int step);

        /// The level of a side, 0 to 3
        int level(FormId reputation, int side);
        /// The answer of GetReputationThreshold: 1 (neutral) when both sides are below the first level, else the class
        /// of the pair of levels (2 to 6) among those that the kind asks about (0 fame and infamy both, 1 fame ahead of
        /// infamy, 2 infamy ahead of fame), and 0 when the pair is not of that kind or the kind is none
        int threshold(FormId reputation, int kind);

        /// The class of a pair of levels, see threshold()
        static int classOf(int fameLevel, int infamyLevel, int kind);
        /// The level of a side that holds the points, for the cap and the three limits as shares of the cap
        static int levelOf(double points, double cap, const std::array<double, 3>& limits);

    private:
        struct Sides
        {
            double mNegative = 0;
            double mPositive = 0;
        };

        std::array<double, 3> limits();
        int levelLocked(FormId reputation, int side, double cap, const std::array<double, 3>& limits);

        ReputationRecords& mRecords;
        std::mutex mMutex;
        std::map<FormId, Sides> mSides;
    };

    /// The section of a saved game that holds the points of the reputations
    constexpr std::uint32_t reputationSectionVersion = 1;
    void writeReputation(Reputation& reputation, SaveWriter& out);
    void readReputation(Reputation& reputation, SaveReader& in, std::uint32_t version);

    /// Gives the commands AddReputation, SetReputation, GetReputation and GetReputationThreshold to the interpreter.
    /// The message is called when AddReputation changes a reputation, to tell the player. Those commands that the
    /// command table does not have are left out.
    void addReputationCommands(
        Interpreter& interpreter, Reputation& reputation, std::function<void(const Reputation::Bump&)> message);
}

#endif
