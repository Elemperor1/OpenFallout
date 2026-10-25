#include "playerprogress.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "arguments.hpp"

namespace FalloutScript
{
    LevellingRules PlayerProgress::defaultRules()
    {
        LevellingRules rules;
        rules.mMaxLevel = 0;
        return rules;
    }

    void PlayerProgress::clear()
    {
        const std::lock_guard lock(mMutex);
        mExperience = 0;
        mKarma = 0;
        mAwardedLevel = 1;
        mSkillPoints = 0;
        mPerkPoints = 0;
        mPerkLevels.clear();
        mMiscStats.clear();
        mPerks.clear();
        mPerkOrder.clear();
        mPerkCount = 0;
        mNotes.clear();
    }

    void PlayerProgress::setRules(const LevellingRules& rules)
    {
        const std::lock_guard lock(mMutex);
        mRules = rules;
    }

    LevellingRules PlayerProgress::rules() const
    {
        const std::lock_guard lock(mMutex);
        return mRules;
    }

    void PlayerProgress::rewardExperience(int amount)
    {
        const std::lock_guard lock(mMutex);
        const std::int64_t sum = static_cast<std::int64_t>(mExperience) + amount;
        std::int64_t most = std::numeric_limits<int>::max();
        if (mRules.mMaxLevel > 0)
            most = Levelling::experienceForLevel(mRules, mRules.mMaxLevel);
        mExperience = static_cast<int>(std::clamp<std::int64_t>(sum, 0, most));
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

    int PlayerProgress::levelUnderLock() const
    {
        return Levelling::levelForExperience(mRules, mExperience);
    }

    int PlayerProgress::level() const
    {
        const std::lock_guard lock(mMutex);
        return levelUnderLock();
    }

    bool PlayerProgress::atHighestLevel() const
    {
        const std::lock_guard lock(mMutex);
        return mRules.mMaxLevel > 0 && levelUnderLock() >= mRules.mMaxLevel;
    }

    int PlayerProgress::experienceForNextLevel() const
    {
        const std::lock_guard lock(mMutex);
        return Levelling::experienceForLevel(mRules, levelUnderLock() + 1);
    }

    void PlayerProgress::setLevel(int level)
    {
        const std::lock_guard lock(mMutex);
        level = std::max(level, 1);
        if (mRules.mMaxLevel > 0)
            level = std::min(level, mRules.mMaxLevel);
        mExperience = Levelling::experienceForLevel(mRules, level);
        mAwardedLevel = level;
    }

    void PlayerProgress::resetExperience()
    {
        const std::lock_guard lock(mMutex);
        mExperience = 0;
        mAwardedLevel = 1;
    }

    std::optional<PlayerProgress::LevelUp> PlayerProgress::awardNextLevel(double intelligence)
    {
        const std::lock_guard lock(mMutex);
        if (mAwardedLevel >= levelUnderLock())
            return std::nullopt;
        ++mAwardedLevel;
        LevelUp up;
        up.mLevel = mAwardedLevel;
        up.mSkillPoints = Levelling::skillPoints(mRules, intelligence, mAwardedLevel);
        up.mPerk = Levelling::givesPerk(mRules, mAwardedLevel);
        mSkillPoints += up.mSkillPoints;
        if (up.mPerk)
        {
            ++mPerkPoints;
            mPerkLevels.push_back(up.mLevel);
        }
        return up;
    }

    int PlayerProgress::skillPoints() const
    {
        const std::lock_guard lock(mMutex);
        return mSkillPoints;
    }

    int PlayerProgress::perkPoints() const
    {
        const std::lock_guard lock(mMutex);
        return mPerkPoints;
    }

    bool PlayerProgress::spendSkillPoints(int amount)
    {
        const std::lock_guard lock(mMutex);
        if (amount < 0 || amount > mSkillPoints)
            return false;
        mSkillPoints -= amount;
        return true;
    }

    bool PlayerProgress::spendPerkPoint()
    {
        const std::lock_guard lock(mMutex);
        if (mPerkPoints <= 0)
            return false;
        --mPerkPoints;
        // The points of the levels are spent in the order they were given
        if (!mPerkLevels.empty())
            mPerkLevels.erase(mPerkLevels.begin());
        return true;
    }

    int PlayerProgress::perkLevel() const
    {
        const std::lock_guard lock(mMutex);
        return mPerkLevels.empty() ? levelUnderLock() : mPerkLevels.front();
    }

    void PlayerProgress::addSkillPoints(int amount)
    {
        const std::lock_guard lock(mMutex);
        mSkillPoints = std::max(mSkillPoints + amount, 0);
    }

    void PlayerProgress::addPerkPoints(int amount)
    {
        const std::lock_guard lock(mMutex);
        mPerkPoints = std::max(mPerkPoints + amount, 0);
        // Points that a script takes away are taken from the latest levels first
        if (mPerkLevels.size() > static_cast<std::size_t>(mPerkPoints))
            mPerkLevels.resize(static_cast<std::size_t>(mPerkPoints));
    }

    void PlayerProgress::modifyMiscStat(int stat, int amount)
    {
        const std::lock_guard lock(mMutex);
        int& value = mMiscStats[stat];
        value = static_cast<int>(
            std::clamp<std::int64_t>(static_cast<std::int64_t>(value) + amount, 0, std::numeric_limits<int>::max()));
    }

    int PlayerProgress::miscStat(int stat) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mMiscStats.find(stat);
        return found != mMiscStats.end() ? found->second : 0;
    }

    void PlayerProgress::rewardKarma(int amount)
    {
        std::function<void(int)> notice;
        {
            const std::lock_guard lock(mMutex);
            const std::int64_t sum = static_cast<std::int64_t>(mKarma) + amount;
            mKarma = static_cast<int>(std::clamp<std::int64_t>(sum, -karmaLimit, karmaLimit));
            notice = mKarmaNotice;
        }
        if (notice && amount != 0)
            notice(amount);
    }

    void PlayerProgress::setKarmaNotice(std::function<void(int)> notice)
    {
        const std::lock_guard lock(mMutex);
        mKarmaNotice = std::move(notice);
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
        // A perk that gains a rank puts the entries of the rank in the list of the games then
        mPerkOrder[{ character, perk }] = ++mPerkCount;
    }

    void PlayerProgress::removePerk(FormId character, FormId perk)
    {
        const std::lock_guard lock(mMutex);
        mPerks.erase({ character, perk });
        mPerkOrder.erase({ character, perk });
    }

    int PlayerProgress::perkRank(FormId character, FormId perk) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mPerks.find({ character, perk });
        return found != mPerks.end() ? found->second : 0;
    }

    std::vector<std::pair<FormId, int>> PlayerProgress::perksOf(FormId character) const
    {
        const std::lock_guard lock(mMutex);
        std::vector<std::pair<std::uint64_t, std::pair<FormId, int>>> found;
        for (auto it = mPerks.lower_bound({ character, 0 }); it != mPerks.end() && it->first.first == character; ++it)
        {
            const auto order = mPerkOrder.find(it->first);
            found.push_back({ order != mPerkOrder.end() ? order->second : 0, { it->first.second, it->second } });
        }
        std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<std::pair<FormId, int>> perks;
        for (const auto& entry : found)
            perks.push_back(entry.second);
        return perks;
    }

    std::vector<FormId> PlayerProgress::perkOwners() const
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> owners;
        for (const auto& [key, rank] : mPerks)
            if (owners.empty() || owners.back() != key.first)
                owners.push_back(key.first);
        return owners;
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
        interpreter.setHandler("GetXPForNextLevel",
            [&progress](CallContext&) { return static_cast<double>(progress.experienceForNextLevel()); });
        // SetLevel and ResetXP are called on a character; the level and the experience kept here are the player's, so
        // a call on anyone else (a script that scales its actor) leaves them alone. SetLevel's other arguments (the
        // level the game scales to, ...) are not used.
        interpreter.setHandler("SetLevel", [&progress](CallContext& call) {
            if (call.mReference == call.mHost.player())
                progress.setLevel(intArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("ResetXP", [&progress](CallContext& call) {
            if (call.mReference == call.mHost.player())
                progress.resetExperience();
            return 0.0;
        });
        interpreter.setHandler("ModPCMiscStat", [&progress](CallContext& call) {
            progress.modifyMiscStat(intArgument(call, 0), intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler(
            "GetPCMiscStat", [&progress](CallContext& call) { return progress.miscStat(intArgument(call, 0)); });
        interpreter.setHandler("GetHasNote",
            [&progress](CallContext& call) { return progress.hasNote(formArgument(call, 0)) ? 1.0 : 0.0; });
    }
}
