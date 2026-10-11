#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLAYERPROGRESS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLAYERPROGRESS_H

#include <map>
#include <mutex>
#include <set>
#include <utility>

#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// What the story gives the player and the characters that nothing else in the game keeps yet: experience and
    /// karma, the perks of each character and the notes of the Pip-Boy. It can be used from any thread. Nothing here
    /// shows them, and a perk has no effect yet. The level of the player is told from the experience; nothing gives
    /// the player the points of a new level.
    class PlayerProgress
    {
    public:
        /// Karma stays between minus this and this
        static constexpr int karmaLimit = 1000;

        /// Forgets everything, as for a new game
        void clear();

        /// Gives experience (or takes it away, down to 0)
        void rewardExperience(int amount);
        int experience() const;
        /// The level that an amount of experience is: 1 for the first 200 points, and one more for each next step,
        /// which is 150 points longer than the one before (200, 350, 500, ...: levels 2, 3, 4 and 5 start at 200, 550,
        /// 1,050 and 1,700). The 150 is the setting iXPBumpBase of both games' files, and the 200 the default of the
        /// GECK's experience settings; the games' own formula is not in their files, so the first step is an
        /// assumption. There is no highest level.
        static int levelFor(int experience);
        int level() const;

        /// Changes the karma, which stays within the limits
        void rewardKarma(int amount);
        int karma() const;

        /// The character gets the perk one rank higher than it has it
        void addPerk(FormId character, FormId perk);
        /// The character loses the perk, whatever its rank
        void removePerk(FormId character, FormId perk);
        /// The rank of the perk the character has, 0 for none
        int perkRank(FormId character, FormId perk) const;

        void addNote(FormId note);
        void removeNote(FormId note);
        bool hasNote(FormId note) const;

    private:
        mutable std::mutex mMutex;
        int mExperience = 0;
        int mKarma = 0;
        std::map<std::pair<FormId, FormId>, int> mPerks;
        std::set<FormId> mNotes;
    };

    /// Gives the commands RewardXP, RewardKarma, AddPerk, RemovePerk, HasPerk, AddNote, RemoveNote and GetHasNote to
    /// the interpreter. Those that the command table does not have are left out.
    void addPlayerProgressCommands(Interpreter& interpreter, PlayerProgress& progress);
}

#endif
