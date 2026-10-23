#include "reputation.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        // The steps of a reputation by the setting that names them, with the value the executables start with
        constexpr std::array<std::pair<std::string_view, double>, 5> bumps{ {
            { "fReputationBumpVeryMinor", 1.0 },
            { "fReputationBumpMinor", 2.0 },
            { "fReputationBumpAverage", 4.0 },
            { "fReputationBumpMajor", 7.0 },
            { "fReputationBumpVeryMajor", 12.0 },
        } };

        constexpr std::array<std::pair<std::string_view, double>, 3> limitSettings{ {
            { "fReputationThresholdOne", 0.15 },
            { "fReputationThresholdTwo", 0.5 },
            { "fReputationThresholdThree", 1.0 },
        } };

        bool isSide(int side)
        {
            return side == 0 || side == 1;
        }
    }

    Reputation::Reputation(ReputationRecords& records)
        : mRecords(records)
    {
    }

    void Reputation::clear()
    {
        const std::lock_guard lock(mMutex);
        mSides.clear();
    }

    std::array<double, 3> Reputation::limits()
    {
        std::array<double, 3> result;
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = mRecords.setting(limitSettings[i].first, limitSettings[i].second);
        return result;
    }

    int Reputation::levelOf(double points, double cap, const std::array<double, 3>& limits)
    {
        // A reputation without a cap is full as soon as it has any points
        const double share = cap > 0 ? points / cap : (points > 0 ? limits[2] : 0.0);
        if (share < limits[0])
            return 0;
        if (share < limits[1])
            return 1;
        if (share < limits[2])
            return 2;
        return 3;
    }

    int Reputation::levelLocked(FormId reputation, int side, double cap, const std::array<double, 3>& limits)
    {
        const auto found = mSides.find(reputation);
        if (found == mSides.end())
            return 0;
        return levelOf(side == 1 ? found->second.mPositive : found->second.mNegative, cap, limits);
    }

    double Reputation::value(FormId reputation, int side)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mSides.find(reputation);
        if (found == mSides.end())
            return 0;
        // Any side that is not the positive one reads the negative one
        return side == 1 ? found->second.mPositive : found->second.mNegative;
    }

    bool Reputation::set(FormId reputation, int side, double points)
    {
        double cap = 0;
        std::string name;
        if (!isSide(side) || !mRecords.reputation(reputation, cap, name))
            return false;
        const std::lock_guard lock(mMutex);
        Sides& sides = mSides[reputation];
        (side == 1 ? sides.mPositive : sides.mNegative) = std::isfinite(points) ? points : 0.0;
        return true;
    }

    Reputation::Bump Reputation::add(FormId reputation, int side, int step)
    {
        Bump result;
        double cap = 0;
        if (!isSide(side) || step < 1 || step > 5 || !mRecords.reputation(reputation, cap, result.mName))
            return result;
        const double amount = mRecords.setting(bumps[step - 1].first, bumps[step - 1].second);
        const std::array<double, 3> limit = limits();
        const std::lock_guard lock(mMutex);
        const int before = levelLocked(reputation, side, cap, limit);
        Sides& sides = mSides[reputation];
        double& points = side == 1 ? sides.mPositive : sides.mNegative;
        // The cap holds the side from above only: a side that scripts put lower stays where it is, and a reputation
        // without a cap (as the levels read it) has none to hold
        points = cap > 0 ? std::min(points + amount, cap) : points + amount;
        result.mApplied = true;
        result.mSide = side;
        result.mLevelChanged = before != levelOf(points, cap, limit);
        result.mPositiveLevel = levelLocked(reputation, 1, cap, limit);
        result.mNegativeLevel = levelLocked(reputation, 0, cap, limit);
        return result;
    }

    int Reputation::level(FormId reputation, int side)
    {
        double cap = 0;
        std::string name;
        if (!isSide(side) || !mRecords.reputation(reputation, cap, name))
            return 0;
        const std::array<double, 3> limit = limits();
        const std::lock_guard lock(mMutex);
        return levelLocked(reputation, side, cap, limit);
    }

    int Reputation::classOf(int fame, int infamy, int kind)
    {
        // Both below the first level is neutral, whatever is asked
        if (fame == 0 && infamy == 0)
            return 1;
        switch (kind)
        {
            case 0:
                // Both sides count: the classes of a person who is known for good and for bad
                if (fame == 1 && infamy == 1)
                    return 3;
                if (fame == 3 && infamy == 2)
                    return 2;
                if (fame == 3 && infamy == 3)
                    return 5;
                if (fame == 2 && infamy == 3)
                    return 2;
                if (fame == 2 && infamy == 2)
                    return 4;
                return 0;
            case 1:
                // Fame ahead of infamy: only fame is 4 to 6 by its level, a little infamy beside it is 2 and 3
                if (infamy == 0 && fame >= 1)
                    return fame == 1 ? 4 : fame == 2 ? 5 : 6;
                if (fame == 2 && infamy == 1)
                    return 2;
                if (fame == 3 && infamy == 1)
                    return 3;
                return 0;
            case 2:
                // The same for infamy
                if (fame == 0 && infamy >= 1)
                    return infamy == 1 ? 4 : infamy == 2 ? 5 : 6;
                if (fame == 1 && infamy == 2)
                    return 2;
                if (fame == 1 && infamy == 3)
                    return 3;
                return 0;
            default:
                return 0;
        }
    }

    int Reputation::threshold(FormId reputation, int kind)
    {
        double cap = 0;
        std::string name;
        if (!mRecords.reputation(reputation, cap, name))
            return 0;
        const std::array<double, 3> limit = limits();
        const std::lock_guard lock(mMutex);
        return classOf(levelLocked(reputation, 1, cap, limit), levelLocked(reputation, 0, cap, limit), kind);
    }

    template <>
    struct SaveAccess<Reputation>
    {
        static void write(Reputation& reputation, SaveWriter& out)
        {
            const std::lock_guard lock(reputation.mMutex);
            out.count(reputation.mSides.size());
            for (const auto& [form, sides] : reputation.mSides)
            {
                out.form(form);
                out.f64(sides.mNegative);
                out.f64(sides.mPositive);
            }
        }

        static void read(Reputation& reputation, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Reputation::Sides> sides;
            const std::size_t count = in.count(20);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> form = in.form();
                Reputation::Sides entry;
                entry.mNegative = in.f64();
                entry.mPositive = in.f64();
                // A reputation of a plugin that is gone is left out
                if (form)
                    sides[*form] = entry;
            }
            const std::lock_guard lock(reputation.mMutex);
            reputation.mSides = std::move(sides);
        }
    };

    void writeReputation(Reputation& reputation, SaveWriter& out)
    {
        SaveAccess<Reputation>::write(reputation, out);
    }

    void readReputation(Reputation& reputation, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Reputation>::read(reputation, in, version);
    }

    void addReputationCommands(
        Interpreter& interpreter, Reputation& reputation, std::function<void(const Reputation::Bump&)> message)
    {
        interpreter.setHandler("AddReputation", [&reputation, message = std::move(message)](CallContext& call) {
            const Reputation::Bump bump
                = reputation.add(formArgument(call, 0), intArgument(call, 1), intArgument(call, 2));
            if (bump.mApplied && message)
                message(bump);
            return 0.0;
        });
        interpreter.setHandler("SetReputation", [&reputation](CallContext& call) {
            reputation.set(formArgument(call, 0), intArgument(call, 1), argument(call, 2));
            return 0.0;
        });
        interpreter.setHandler("GetReputation",
            [&reputation](CallContext& call) { return reputation.value(formArgument(call, 0), intArgument(call, 1)); });
        interpreter.setHandler("GetReputationThreshold", [&reputation](CallContext& call) {
            return static_cast<double>(reputation.threshold(formArgument(call, 0), intArgument(call, 1)));
        });
    }
}
