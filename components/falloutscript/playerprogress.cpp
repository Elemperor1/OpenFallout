#include "playerprogress.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "arguments.hpp"

namespace FalloutScript
{
    void PlayerProgress::clear()
    {
        const std::lock_guard lock(mMutex);
        mExperience = 0;
        mKarma = 0;
        mPerks.clear();
        mNotes.clear();
    }

    void PlayerProgress::rewardExperience(int amount)
    {
        const std::lock_guard lock(mMutex);
        const std::int64_t sum = static_cast<std::int64_t>(mExperience) + amount;
        mExperience = static_cast<int>(std::clamp<std::int64_t>(sum, 0, std::numeric_limits<int>::max()));
    }

    int PlayerProgress::experience() const
    {
        const std::lock_guard lock(mMutex);
        return mExperience;
    }

    int PlayerProgress::levelFor(int experience)
    {
        // The experience that level n needs is the sum of the steps 200, 350, 500, ...: (n - 1) * (75 * n + 50)
        const auto needed = [](std::int64_t n) { return (n - 1) * (75 * n + 50); };
        const double points = std::max(experience, 0);
        std::int64_t level = static_cast<std::int64_t>((25 + std::sqrt(625 + 300 * (50 + points))) / 150);
        while (level > 1 && needed(level) > experience)
            --level;
        while (needed(level + 1) <= experience)
            ++level;
        return static_cast<int>(std::max<std::int64_t>(level, 1));
    }

    int PlayerProgress::level() const
    {
        return levelFor(experience());
    }

    void PlayerProgress::rewardKarma(int amount)
    {
        const std::lock_guard lock(mMutex);
        const std::int64_t sum = static_cast<std::int64_t>(mKarma) + amount;
        mKarma = static_cast<int>(std::clamp<std::int64_t>(sum, -karmaLimit, karmaLimit));
    }

    int PlayerProgress::karma() const
    {
        const std::lock_guard lock(mMutex);
        return mKarma;
    }

    void PlayerProgress::addPerk(FormId character, FormId perk)
    {
        if (character == 0 || perk == 0)
            return;
        const std::lock_guard lock(mMutex);
        int& rank = mPerks[{ character, perk }];
        if (rank < std::numeric_limits<int>::max())
            ++rank;
    }

    void PlayerProgress::removePerk(FormId character, FormId perk)
    {
        const std::lock_guard lock(mMutex);
        mPerks.erase({ character, perk });
    }

    int PlayerProgress::perkRank(FormId character, FormId perk) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mPerks.find({ character, perk });
        return found != mPerks.end() ? found->second : 0;
    }

    void PlayerProgress::addNote(FormId note)
    {
        if (note == 0)
            return;
        const std::lock_guard lock(mMutex);
        mNotes.insert(note);
    }

    void PlayerProgress::removeNote(FormId note)
    {
        const std::lock_guard lock(mMutex);
        mNotes.erase(note);
    }

    bool PlayerProgress::hasNote(FormId note) const
    {
        const std::lock_guard lock(mMutex);
        return mNotes.count(note) != 0;
    }

    void addPlayerProgressCommands(Interpreter& interpreter, PlayerProgress& progress)
    {
        interpreter.setHandler("RewardXP", [&progress](CallContext& call) {
            progress.rewardExperience(intArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("RewardKarma", [&progress](CallContext& call) {
            progress.rewardKarma(intArgument(call, 0));
            return 0.0;
        });
        // The second argument of AddPerk says whether the perk goes to the companions of the player too, which has no
        // companions to go to here
        interpreter.setHandler("AddPerk", [&progress](CallContext& call) {
            progress.addPerk(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        // RemovePerk takes the same second argument as AddPerk, and the perk with it, whatever its rank
        interpreter.setHandler("RemovePerk", [&progress](CallContext& call) {
            progress.removePerk(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        // The second argument of HasPerk says whether to ask about the companions of the player instead, which has no
        // companions to ask about
        interpreter.setHandler("HasPerk", [&progress](CallContext& call) {
            return call.mReference != 0 && progress.perkRank(call.mReference, formArgument(call, 0)) > 0 ? 1.0 : 0.0;
        });
        interpreter.setHandler("AddNote", [&progress](CallContext& call) {
            progress.addNote(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("RemoveNote", [&progress](CallContext& call) {
            progress.removeNote(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("GetHasNote",
            [&progress](CallContext& call) { return progress.hasNote(formArgument(call, 0)) ? 1.0 : 0.0; });
    }
}
