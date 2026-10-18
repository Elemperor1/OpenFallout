#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLAYERPROGRESS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLAYERPROGRESS_H

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "levelling.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// What the story gives the player and the characters that nothing else in the game keeps yet: experience and
    /// karma, the perks of each character and the notes of the Pip-Boy, and what levelling up gives (skill points
    /// and perks to choose, as LevellingRules says). It can be used from any thread. The level of the player is told
    /// from the experience; the points of a new level are handed out by awardNextLevel.
    class PlayerProgress
    {
    public:
        /// Karma stays between minus this and this
        static constexpr int karmaLimit = 1000;

        /// Forgets everything, as for a new game
        void clear();

        /// How levelling up works (see LevellingRules). Until the host sets them there is no highest level and
        /// the experience of a level is 200 for the first step and 150 more for each next one.
        void setRules(const LevellingRules& rules);
        LevellingRules rules() const;

        /// Gives experience (or takes it away, down to 0), up to what the highest level needs
        void rewardExperience(int amount);
        int experience() const;
        /// The level that an amount of experience is under the default rules: 1 for the first 200 points, and one more
        /// for each next step, which is 150 points longer than the one before (200, 350, 500, ...: levels 2, 3, 4 and
        /// 5 start at 200, 550, 1,050 and 1,700).
        static int levelFor(int experience);
        /// The level of the player under the rules that are set
        int level() const;
        /// Whether the player is at the highest level of the rules (never when they have none)
        bool atHighestLevel() const;
        /// The experience that the next level needs (the same experience when the highest level is reached)
        int experienceForNextLevel() const;
        /// Puts the player at that level at once (SetLevel): the experience becomes what the level starts at, and
        /// the points of the levels before are taken as given
        void setLevel(int level);
        /// Takes all the experience, as for ResetXP
        void resetExperience();

        /// The level up that has not been handed out yet: the level the player reached, the skill points and the
        /// perk it gives. None when the points of every level reached have been given.
        struct LevelUp
        {
            int mLevel = 0;
            int mSkillPoints = 0;
            bool mPerk = false;
        };
        /// Hands out the points of the next level that was reached and has not been given (for a player with that
        /// intelligence) and returns it
        std::optional<LevelUp> awardNextLevel(double intelligence);
        /// Skill points and perks that the player can still spend
        int skillPoints() const;
        int perkPoints() const;
        /// Spends points; false when there are not that many
        bool spendSkillPoints(int amount);
        bool spendPerkPoint();
        /// Adds points that a script or an effect gives (a negative amount takes them, down to none)
        void addSkillPoints(int amount);
        void addPerkPoints(int amount);

        /// The counters of the Pip-Boy (ModPCMiscStat, GetPCMiscStat)
        void modifyMiscStat(int stat, int amount);
        int miscStat(int stat) const;

        /// Changes the karma, which stays within the limits
        void rewardKarma(int amount);
        int karma() const;

        /// The character gets the perk one rank higher than it has it
        void addPerk(FormId character, FormId perk);
        /// The character loses the perk, whatever its rank
        void removePerk(FormId character, FormId perk);
        /// The rank of the perk the character has, 0 for none
        int perkRank(FormId character, FormId perk) const;
        /// The perks of the character with their ranks, the one gained (or last raised) longest ago first
        std::vector<std::pair<FormId, int>> perksOf(FormId character) const;
        /// The characters that have a perk
        std::vector<FormId> perkOwners() const;

        void addNote(FormId note);
        void removeNote(FormId note);
        bool hasNote(FormId note) const;

    private:
        /// No highest level, and the first step of 200
        static LevellingRules defaultRules();
        int levelUnderLock() const;

        mutable std::mutex mMutex;
        LevellingRules mRules = defaultRules();
        int mExperience = 0;
        int mKarma = 0;
        /// The level up to which the points have been handed out
        int mAwardedLevel = 1;
        int mSkillPoints = 0;
        int mPerkPoints = 0;
        std::map<int, int> mMiscStats;
        std::map<std::pair<FormId, FormId>, int> mPerks;
        /// When each perk was gained or last gained a rank (a count), which is the order of its entries in a list
        std::map<std::pair<FormId, FormId>, std::uint64_t> mPerkOrder;
        std::uint64_t mPerkCount = 0;
        std::set<FormId> mNotes;
    };

    /// Gives the commands RewardXP, RewardKarma, AddPerk, RemovePerk, HasPerk, AddNote, RemoveNote and GetHasNote to
    /// the interpreter, and GetXPForNextLevel, SetLevel, ResetXP, ModPCMiscStat and GetPCMiscStat. Those that the
    /// command table does not have are left out.
    void addPlayerProgressCommands(Interpreter& interpreter, PlayerProgress& progress);
}

#endif
