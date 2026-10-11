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
    /// shows them or changes the level of the player, and a perk has no effect yet.
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
