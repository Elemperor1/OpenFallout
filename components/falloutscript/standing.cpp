#include "standing.hpp"

#include <algorithm>
#include <cmath>

#include "actors.hpp"
#include "actortraits.hpp"
#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        // The executables' float to integer conversion cuts off the fraction
        int cut(double value)
        {
            if (std::isnan(value))
                return 0;
            return static_cast<int>(std::trunc(std::clamp(value, -2147483648.0, 2147483647.0)));
        }

        constexpr int noMaximum = -100000;
        constexpr int noMinimum = 100000;

        bool member(const std::vector<std::pair<FormId, int>>& factions, FormId faction, int& rank)
        {
            for (const auto& [form, has] : factions)
                if (form == faction)
                {
                    rank = has;
                    return true;
                }
            return false;
        }
    }

    StandingSettings StandingSettings::fromGameSettings(const Lookup& lookup)
    {
        StandingSettings settings;
        if (!lookup)
            return settings;
        const auto take = [&lookup](std::string_view name, double& setting) {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        };
        take("fDispTargetCharBase", settings.mCharBase);
        take("fDispTargetCharMult", settings.mCharMult);
        take("fDispTargetCharOffset", settings.mCharOffset);
        take("fDispTargetKarmaMult", settings.mKarmaMult);
        take("fDispActorRespBase", settings.mResponsibilityBase);
        take("fDispActorRespMult", settings.mResponsibilityMult);
        take("fDispTargetRaceMult", settings.mRaceMult);
        take("fDispTargetFactionMult", settings.mFactionMult);
        take("fDispTargetFactionRankBase", settings.mFactionRankBase);
        take("fDispTargetWeaponBase", settings.mWeaponBase);
        take("fDispTargetDiseaseBase", settings.mDiseaseBase);
        take("fAlignEvilMaxKarma", settings.mEvilMax);
        take("fAlignVeryEvilMaxKarma", settings.mVeryEvilMax);
        take("fAlignGoodMinKarma", settings.mGoodMin);
        take("fAlignVeryGoodMinKarma", settings.mVeryGoodMin);
        if (const std::optional<double> value = lookup("iDialogueDispositionFriendValue"))
            settings.mFriendValue = cut(*value);
        return settings;
    }

    Standing::Standing(StandingRecords& records)
        : mRecords(records)
    {
    }

    void Standing::clear()
    {
        const std::lock_guard lock(mMutex);
        mPairs.clear();
        mReactions.clear();
    }

    int Standing::formula(const StandingSettings& settings, const DispositionInput& input, int pairModifier)
    {
        // The charisma, the karma and the responsibility are whole numbers in the executables
        const double charisma = std::trunc(input.mCharisma);
        const double karma = std::trunc(input.mKarma);
        const double responsibility = std::trunc(input.mResponsibility);
        int total = input.mBase;
        total += cut((charisma - settings.mCharOffset) * settings.mCharMult + settings.mCharBase);
        if (!input.mCreature)
            total += cut(settings.mKarmaMult * karma
                * (settings.mResponsibilityBase + settings.mResponsibilityMult * responsibility));
        total += cut(settings.mRaceMult) * input.mRace;
        // The executable gives the faction term the flag of the pair (1, or -1 when no pair was found) and the reaction
        // (0 then); the setting of the multiplier is read twice
        const int reaction = input.mFactionPair ? input.mFactionReaction : 0;
        const double found = input.mFactionPair ? 1.0 : -1.0;
        total += cut(settings.mFactionMult * reaction * (settings.mFactionRankBase + settings.mFactionMult * found));
        if (!input.mCreature)
            total += cut(settings.mWeaponBase) * (input.mWeaponDrawn ? 1 : 0);
        // The term of the diseases is always multiplied by 0 (the number of diseases is not counted)
        total += pairModifier;
        return total;
    }

    int Standing::disposition(
        FormId self, FormId other, const DispositionInput& input, const std::function<double(double)>& adjust)
    {
        if (other == 0)
            return 0;
        if (input.mSame)
            return 100;
        double value = formula(mRecords.standingSettings(), input, pairModifier(self, other));
        if (adjust)
            value = adjust(value);
        return std::clamp(cut(value), 0, 100);
    }

    int Standing::pairModifier(FormId self, FormId other)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mPairs.find({ self, other });
        return found == mPairs.end() ? 0 : found->second;
    }

    void Standing::modifyDisposition(FormId self, FormId other, double delta, int current)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mPairs.find({ self, other });
        // A pair that has no entry yet gets the change as it is; one that has is not taken past the ends
        if (found == mPairs.end())
        {
            mPairs[{ self, other }] = cut(delta);
            return;
        }
        double change = delta;
        const double total = current + change;
        if (total < 0)
            change -= total;
        else if (total > 100)
        {
            const int over = cut(change) + current - 100;
            change = change <= over ? 0.0 : change - over;
        }
        found->second += cut(change);
    }

    int Standing::factionReaction(FormId faction, FormId other)
    {
        {
            const std::lock_guard lock(mMutex);
            const auto found = mReactions.find({ faction, other });
            if (found != mReactions.end())
                return found->second;
        }
        return mRecords.recordedReaction(faction, other).value_or(0);
    }

    void Standing::setFactionReaction(FormId faction, FormId other, int value)
    {
        const std::lock_guard lock(mMutex);
        mReactions[{ faction, other }] = value;
    }

    void Standing::modFactionReaction(FormId faction, FormId other, int amount)
    {
        const int now = factionReaction(faction, other);
        setFactionReaction(faction, other, now + amount);
    }

    Standing::PairReaction Standing::pairReaction(
        const std::vector<FormId>& selfFactions, const std::vector<FormId>& otherFactions)
    {
        PairReaction result;
        int highest = noMaximum;
        int lowest = noMinimum;
        for (const FormId first : selfFactions)
        {
            if (first == 0)
                continue;
            for (const FormId second : otherFactions)
            {
                if (second == 0)
                    continue;
                const int reaction = factionReaction(first, second);
                if (first == second)
                {
                    if (reaction > highest)
                    {
                        highest = reaction;
                        result = { true, reaction };
                    }
                }
                else if (highest == noMaximum && reaction < lowest)
                {
                    lowest = reaction;
                    result = { true, reaction };
                }
            }
        }
        return result;
    }

    int Standing::karmaClass(const StandingSettings& settings, double karma)
    {
        if (karma <= settings.mEvilMax)
            return karma > settings.mVeryEvilMax ? 2 : 4;
        if (karma >= settings.mGoodMin)
            return karma >= settings.mVeryGoodMin ? 3 : 0;
        return 1;
    }

    bool Standing::actorEvil(const std::vector<FormId>& factions, const std::function<bool(FormId)>& evilFaction)
    {
        return !factions.empty()
            && std::all_of(factions.begin(), factions.end(), [&evilFaction](FormId faction) { return evilFaction(faction); });
    }

    template <>
    struct SaveAccess<Standing>
    {
        static void writePairs(SaveWriter& out, const std::map<std::pair<FormId, FormId>, int>& pairs)
        {
            out.count(pairs.size());
            for (const auto& [key, value] : pairs)
            {
                out.form(key.first);
                out.form(key.second);
                out.i32(value);
            }
        }

        static std::map<std::pair<FormId, FormId>, int> readPairs(SaveReader& in)
        {
            std::map<std::pair<FormId, FormId>, int> pairs;
            const std::size_t count = in.count(12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> first = in.form();
                const std::optional<FormId> second = in.form();
                const int value = in.i32();
                if (first && second)
                    pairs[{ *first, *second }] = value;
            }
            return pairs;
        }

        static void write(Standing& standing, SaveWriter& out)
        {
            const std::lock_guard lock(standing.mMutex);
            writePairs(out, standing.mPairs);
            writePairs(out, standing.mReactions);
        }

        static void read(Standing& standing, SaveReader& in, std::uint32_t)
        {
            auto pairs = readPairs(in);
            auto reactions = readPairs(in);
            const std::lock_guard lock(standing.mMutex);
            standing.mPairs = std::move(pairs);
            standing.mReactions = std::move(reactions);
        }
    };

    void writeStanding(Standing& standing, SaveWriter& out)
    {
        SaveAccess<Standing>::write(standing, out);
    }

    void readStanding(Standing& standing, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Standing>::read(standing, in, version);
    }

    void addStandingCommands(Interpreter& interpreter, Standing& standing, StandingRecords& records,
        StandingWorld& world, ActorState& actors, ActorTraitRecords& traits)
    {
        const auto dispositionOf = [&standing, &world](FormId self, FormId other) {
            DispositionInput input;
            if (self == 0 || other == 0 || !world.dispositionInput(self, other, input))
                return 0;
            return standing.disposition(self, other, input,
                [&world, self, other](double value) { return world.adjustDisposition(self, other, value); });
        };
        interpreter.setHandler("GetDisposition", [dispositionOf](CallContext& call) {
            return static_cast<double>(dispositionOf(call.mReference, formArgument(call, 0)));
        });
        // Changes what the character thinks of the other; the amount is kept between what 0 and 100 allow
        interpreter.setHandler("ModDisposition", [&standing, dispositionOf](CallContext& call) {
            const FormId other = formArgument(call, 0);
            if (call.mReference != 0 && other != 0)
                standing.modifyDisposition(call.mReference, other, argument(call, 1), dispositionOf(call.mReference, other));
            return 0.0;
        });
        interpreter.setHandler("SetDisposition", [&standing, dispositionOf](CallContext& call) {
            const FormId other = formArgument(call, 0);
            if (call.mReference != 0 && other != 0)
            {
                const int now = dispositionOf(call.mReference, other);
                standing.modifyDisposition(call.mReference, other, argument(call, 1) - now, now);
            }
            return 0.0;
        });
        interpreter.setHandler("HasFriendDisposition", [&records, &world, dispositionOf](CallContext& call) {
            return call.mReference != 0
                    && dispositionOf(call.mReference, world.player()) >= records.standingSettings().mFriendValue
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("FriendDispositionBoost", [&standing, &records, &world, dispositionOf](CallContext& call) {
            const FormId player = world.player();
            if (call.mReference == 0 || player == 0)
                return 0.0;
            const int now = dispositionOf(call.mReference, player);
            const int friendValue = records.standingSettings().mFriendValue;
            if (now < friendValue)
                standing.modifyDisposition(call.mReference, player, friendValue - now, now);
            return 0.0;
        });
        // The executable of New Vegas answers 0 to both (the evaluation functions hold no rule)
        interpreter.setHandler("GetPersuasionNumber", [](CallContext&) { return 0.0; });
        interpreter.setHandler("GetTotalPersuasionNumber", [](CallContext&) { return 0.0; });

        interpreter.setHandler("GetFactionRelation", [&world](CallContext& call) {
            const FormId other = formArgument(call, 0);
            return call.mReference != 0 && other != 0 ? static_cast<double>(world.relation(call.mReference, other)) : 0.0;
        });
        interpreter.setHandler("GetFactionReaction", [&standing](CallContext& call) {
            const FormId first = formArgument(call, 0);
            const FormId second = formArgument(call, 1);
            return first != 0 && second != 0 ? static_cast<double>(standing.factionReaction(first, second)) : 0.0;
        });
        interpreter.setHandler("SetFactionReaction", [&standing](CallContext& call) {
            const FormId first = formArgument(call, 0);
            const FormId second = formArgument(call, 1);
            if (first != 0 && second != 0)
                standing.setFactionReaction(first, second, intArgument(call, 2));
            return 0.0;
        });
        interpreter.setHandler("ModFactionReaction", [&standing](CallContext& call) {
            const FormId first = formArgument(call, 0);
            const FormId second = formArgument(call, 1);
            if (first != 0 && second != 0)
                standing.modFactionReaction(first, second, intArgument(call, 2));
            return 0.0;
        });

        // The difference of the ranks of the character and another in a faction; 0 when either is not in it
        interpreter.setHandler("GetFactionRankDifference", [&actors](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            const FormId other = formArgument(call, 1);
            std::vector<std::pair<FormId, int>> mine;
            std::vector<std::pair<FormId, int>> theirs;
            int mineRank = -1;
            int theirRank = -1;
            if (faction == 0 || call.mReference == 0 || other == 0 || !actors.factions(call.mReference, mine)
                || !actors.factions(other, theirs) || !member(mine, faction, mineRank)
                || !member(theirs, faction, theirRank))
                return 0.0;
            return static_cast<double>(mineRank - theirRank);
        });
        // The rank of a member changes by the amount and does not go below 0 (0x8B86E0)
        interpreter.setHandler("ModFactionRank", [&actors](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            std::vector<std::pair<FormId, int>> mine;
            int rank = -1;
            if (faction != 0 && call.mReference != 0 && actors.factions(call.mReference, mine)
                && member(mine, faction, rank) && rank >= 0)
                actors.setFactionRank(call.mReference, faction, std::max(rank + intArgument(call, 1), 0));
            return 0.0;
        });
        interpreter.setHandler("RemoveFromAllFactions", [&actors](CallContext& call) {
            std::vector<std::pair<FormId, int>> mine;
            if (call.mReference != 0 && actors.factions(call.mReference, mine))
                for (const auto& [faction, rank] : mine)
                    actors.removeFromFaction(call.mReference, faction);
            return 0.0;
        });
        const auto shareFaction = [&actors](FormId first, FormId second) {
            std::vector<std::pair<FormId, int>> mine;
            std::vector<std::pair<FormId, int>> theirs;
            if (first == 0 || second == 0 || !actors.factions(first, mine) || !actors.factions(second, theirs))
                return false;
            int rank = 0;
            return std::any_of(mine.begin(), mine.end(),
                [&](const std::pair<FormId, int>& faction) { return member(theirs, faction.first, rank); });
        };
        interpreter.setHandler("SameFaction", [shareFaction](CallContext& call) {
            return shareFaction(call.mReference, formArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SameFactionAsPC", [shareFaction, &world](CallContext& call) {
            return shareFaction(call.mReference, world.player()) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetPCInFaction", [&actors, &world](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            std::vector<std::pair<FormId, int>> mine;
            int rank = -1;
            return faction != 0 && actors.factions(world.player(), mine) && member(mine, faction, rank) && rank >= 0
                ? 1.0
                : 0.0;
        });

        const auto sameTrait = [&traits](FormId first, FormId second, bool race) {
            ActorTraits a;
            ActorTraits b;
            if (first == 0 || second == 0 || !traits.traitsOf(first, a) || !traits.traitsOf(second, b))
                return false;
            return race ? a.mRace != 0 && a.mRace == b.mRace : a.mFemale == b.mFemale;
        };
        interpreter.setHandler("SameRace", [sameTrait](CallContext& call) {
            return sameTrait(call.mReference, formArgument(call, 0), true) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SameSex", [sameTrait](CallContext& call) {
            return sameTrait(call.mReference, formArgument(call, 0), false) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SameRaceAsPC", [sameTrait, &world](CallContext& call) {
            return sameTrait(call.mReference, world.player(), true) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SameSexAsPC", [sameTrait, &world](CallContext& call) {
            return sameTrait(call.mReference, world.player(), false) ? 1.0 : 0.0;
        });

        interpreter.setHandler("GetIsAlignment", [&records, &world](CallContext& call) {
            return call.mReference != 0
                    && Standing::karmaClass(records.standingSettings(), world.karmaOf(call.mReference)) == intArgument(call, 0)
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("IsActorEvil", [&world](CallContext& call) {
            return call.mReference != 0 && world.isEvil(call.mReference) ? 1.0 : 0.0;
        });
    }
}
