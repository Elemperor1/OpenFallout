#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_OBJECTSTATE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_OBJECTSTATE_H

#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// What the records of the game say about the things that scripts mark
    class ObjectStateRecords
    {
    public:
        virtual ~ObjectStateRecords() = default;

        /// Whether the reference is a character or a creature
        virtual bool isActor(FormId reference) = 0;
        /// The reference of the player
        virtual FormId player() = 0;
        /// The character the player is, which is the owner that scripts mean when they name none
        virtual FormId playerBase() = 0;
        /// The form an owner stands for: the character that a reference of one is of, any other form as it is
        virtual FormId ownerForm(FormId form) = 0;
        /// The owner that the record of the reference gives it, 0 for none
        virtual FormId recordOwner(FormId reference) = 0;
        /// The owner that the record of the cell gives it, 0 for none
        virtual FormId recordCellOwner(FormId cell) = 0;
        /// Whether the record of the cell makes it a public place
        virtual bool recordCellPublic(FormId cell) = 0;
    };

    /// What scripts mark: whose a thing is, which characters crime does not touch, which are ghosts, which things the
    /// player must not lose, and who follows the player. Every flag starts as the records say and is kept as scripts
    /// made it. Nothing in the game reads these yet but the questions scripts ask; the games read them in the
    /// reporting of crimes, in what the artificial intelligence notices and targets, in what a package does and in the
    /// inventory, none of which has been made. It can be used from any thread.
    class ObjectState
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit ObjectState(ObjectStateRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        /// The owner of a reference, 0 for none
        FormId owner(FormId reference);
        void setOwner(FormId reference, FormId owner);
        /// Whether the form is the owner of the reference. Only the owner it has itself counts: a reference of a place
        /// that someone owns is no one's, and a member of the faction that owns a reference is not its owner.
        bool isOwner(FormId reference, FormId candidate);

        FormId cellOwner(FormId cell);
        void setCellOwner(FormId cell, FormId owner);
        bool cellPublic(FormId cell);
        void setCellPublic(FormId cell, bool isPublic);

        bool ignoresCrime(FormId actor);
        void setIgnoresCrime(FormId actor, bool ignores);
        bool ghost(FormId actor);
        void setGhost(FormId actor, bool isGhost);
        /// Whether the base form is marked as one the player must keep
        bool questObject(FormId base);
        void setQuestObject(FormId base, bool marked);
        bool teammate(FormId actor);
        void setTeammate(FormId actor, bool isTeammate);
        /// The characters that follow the player as teammates
        std::vector<FormId> teammates();
        /// Whether the faction has been made an enemy of the player
        bool enemyOfFaction(FormId faction);
        void setEnemyOfFaction(FormId faction, bool enemy);
        /// The combat style a script gave the character, 0 for none
        FormId combatStyle(FormId actor);
        void setCombatStyle(FormId actor, FormId style);

    private:
        ObjectStateRecords& mRecords;
        std::mutex mMutex;
        std::map<FormId, FormId> mOwners;
        std::map<FormId, FormId> mCellOwners;
        std::map<FormId, bool> mCellPublic;
        std::set<FormId> mIgnoreCrime;
        std::set<FormId> mGhosts;
        std::map<FormId, bool> mQuestObjects;
        std::set<FormId> mTeammates;
        std::set<FormId> mEnemyFactions;
        std::map<FormId, FormId> mCombatStyles;
    };

    /// The section of a saved game that holds what scripts marked
    constexpr std::uint32_t objectStateSectionVersion = 1;
    void writeObjectState(ObjectState& state, SaveWriter& out);
    void readObjectState(ObjectState& state, SaveReader& in, std::uint32_t version);

    /// Gives the commands SetOwnership, IsOwner, SetCellOwnership, SetCellPublicFlag, IgnoreCrime, GetIgnoreCrime,
    /// SetGhost, GetIsGhost, SetQuestObject, SetPlayerTeammate, GetPlayerTeammate, SetPCEnemyofFaction,
    /// GetPCEnemyofFaction and SetCombatStyle to the interpreter, and GetCauseofDeath, GetDestructionStage and
    /// HasBeenEaten with the answer that the executables give when they have no data (the games keep a cause of death
    /// and the stage of what can be destroyed, which are not made; nothing here ever feeds on a dead character). Those
    /// that the command table does not have are left out.
    void addObjectStateCommands(Interpreter& interpreter, ObjectState& state, ObjectStateRecords& records);
}

#endif
