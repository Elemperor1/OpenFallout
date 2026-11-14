#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_STANDING_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_STANDING_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    class ActorState;
    class ActorTraitRecords;

    /// The numbers that what people think of the player goes by: the settings of the formula of the disposition
    /// (0x642A60) and the limits of the classes of karma (0x47E040). Both executables start them with the same values.
    struct StandingSettings
    {
        // fDispTargetCharBase, fDispTargetCharMult (the data files make it 10), fDispTargetCharOffset
        double mCharBase = 0.0;
        double mCharMult = 5.0;
        double mCharOffset = 5.0;
        // fDispTargetKarmaMult, fDispActorRespBase, fDispActorRespMult
        double mKarmaMult = 0.002;
        double mResponsibilityBase = -50.0;
        double mResponsibilityMult = 1.0;
        // fDispTargetRaceMult
        double mRaceMult = 1.0;
        // fDispTargetFactionMult (the executable uses it twice) and fDispTargetFactionRankBase (fDispTargetFactionRankMult
        // is read by nothing)
        double mFactionMult = 1.0;
        double mFactionRankBase = 1.0;
        // fDispTargetWeaponBase and fDispTargetDiseaseBase
        double mWeaponBase = -10.0;
        double mDiseaseBase = 0.0;
        /// iDialogueDispositionFriendValue: the disposition from which someone is a friend
        int mFriendValue = 50;
        // fAlignEvilMaxKarma, fAlignVeryEvilMaxKarma, fAlignGoodMinKarma, fAlignVeryGoodMinKarma
        double mEvilMax = -250.0;
        double mVeryEvilMax = -750.0;
        double mGoodMin = 250.0;
        double mVeryGoodMin = 750.0;

        using Lookup = std::function<std::optional<double>(std::string_view)>;
        static StandingSettings fromGameSettings(const Lookup& lookup);
    };

    /// What the formula of the disposition is given about the two characters (the one whose disposition it is, the
    /// "self", and the one it is toward, the "other"; the other is the player in the dialogues)
    struct DispositionInput
    {
        /// The disposition that the record of self gives it (the base of its ACBS)
        int mBase = 0;
        /// The charisma of the other and its karma, and the responsibility of self
        double mCharisma = 5;
        double mKarma = 0;
        double mResponsibility = 0;
        /// What the race of self makes of the race of the other (the modifier of the race's list)
        int mRace = 0;
        /// Whether a pair of factions of the two was found, and how they stand (see Standing::factionReaction)
        bool mFactionPair = false;
        int mFactionReaction = 0;
        /// Whether the other has a weapon drawn
        bool mWeaponDrawn = false;
        /// Self is a creature (no karma and no weapon term)
        bool mCreature = false;
        /// Self and the other are the same character (the answer is 100 then)
        bool mSame = false;
    };

    /// What the records say that the standing needs
    class StandingRecords
    {
    public:
        virtual ~StandingRecords() = default;

        virtual StandingSettings standingSettings() = 0;
        /// What a faction thinks of another by the record: the modifier of its list of relations (nothing when the list
        /// has no entry)
        virtual std::optional<int> recordedReaction(FormId faction, FormId other) = 0;
    };

    /// What people think of the player and of each other, as the executables work it: the disposition of one character
    /// toward another (the record's base, the charisma and karma of the other, race, faction, a drawn weapon, and what
    /// has happened between the two), the reactions of the factions to each other, and the classes of karma.
    /// It can be used from any thread.
    class Standing
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit Standing(StandingRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        /// The sum of the terms of the formula of the disposition (0x642A60), the truncation of each as the
        /// executable's float to integer conversion does, with the pair modifier that is added last
        static int formula(const StandingSettings& settings, const DispositionInput& input, int pairModifier);

        /// The disposition of self toward other (0x87FD90): 100 for the same character, else the formula and the
        /// perk entry that `adjust` stands for, kept between 0 and 100. `adjust` may be empty.
        int disposition(FormId self, FormId other, const DispositionInput& input,
            const std::function<double(double)>& adjust = {});

        /// What has happened between the two: the number that is added to the formula (0x87FCB0)
        int pairModifier(FormId self, FormId other);
        /// ModDisposition (0x87FB40): changes what self thinks of other by `delta`, so that the disposition stays
        /// between 0 and 100: `current` is the disposition now
        void modifyDisposition(FormId self, FormId other, double delta, int current);

        /// What a faction thinks of another: what a script made of it, else what the record says, else nothing (0)
        int factionReaction(FormId faction, FormId other);
        void setFactionReaction(FormId faction, FormId other, int value);
        void modFactionReaction(FormId faction, FormId other, int amount);

        /// How two characters stand by their factions (0x8B7FE0): a faction they share counts over any other, the highest
        /// reaction of a faction to itself winning; with none shared, the lowest reaction among the other pairs
        struct PairReaction
        {
            bool mFound = false;
            int mReaction = 0;
        };
        PairReaction pairReaction(const std::vector<FormId>& selfFactions, const std::vector<FormId>& otherFactions);

        /// The class of a karma (0x47E040): 0 good, 1 neutral, 2 evil, 3 very good, 4 very evil
        static int karmaClass(const StandingSettings& settings, double karma);

        /// IsActorEvil: the character has factions and all of them are evil (0x47D740)
        static bool actorEvil(const std::vector<FormId>& factions, const std::function<bool(FormId)>& evilFaction);

    private:
        StandingRecords& mRecords;
        std::mutex mMutex;
        std::map<std::pair<FormId, FormId>, int> mPairs;
        /// What scripts made of the reactions of the factions (the value that the faction now has)
        std::map<std::pair<FormId, FormId>, int> mReactions;
    };

    /// The section of a saved game that holds the standing
    constexpr std::uint32_t standingSectionVersion = 1;
    void writeStanding(Standing& standing, SaveWriter& out);
    void readStanding(Standing& standing, SaveReader& in, std::uint32_t version);

    /// What the commands about standing need of the world
    class StandingWorld
    {
    public:
        virtual ~StandingWorld() = default;

        virtual FormId player() = 0;
        /// What the formula is given about the two characters. False when either is not known.
        virtual bool dispositionInput(FormId self, FormId other, DispositionInput& input) = 0;
        /// The entry of the perks that adjusts the disposition (14 of the entry points), which the host runs when it has
        /// them
        virtual double adjustDisposition(FormId /*self*/, FormId /*other*/, double value) { return value; }
        /// How self stands to the other by the factions (GetFactionRelation): 0 neutral, 1 enemy, 2 ally, 3 friend
        virtual int relation(FormId self, FormId other) = 0;
        /// The karma of the character, which the alignment goes by
        virtual double karmaOf(FormId actor) = 0;
        /// Whether the character is evil
        virtual bool isEvil(FormId actor) = 0;
    };

    /// Gives the commands about standing to the interpreter: GetDisposition, ModDisposition, SetDisposition,
    /// HasFriendDisposition, FriendDispositionBoost, GetPersuasionNumber, GetTotalPersuasionNumber, GetFactionRelation,
    /// GetFactionReaction, SetFactionReaction, ModFactionReaction, GetFactionRankDifference, ModFactionRank,
    /// RemoveFromAllFactions, SameFaction, SameFactionAsPC, GetPCInFaction, SameRace, SameSex, SameRaceAsPC,
    /// SameSexAsPC, GetIsAlignment and IsActorEvil. Those that the command table does not have are left out.
    void addStandingCommands(Interpreter& interpreter, Standing& standing, StandingRecords& records,
        StandingWorld& world, ActorState& actors, ActorTraitRecords& traits);
}

#endif
