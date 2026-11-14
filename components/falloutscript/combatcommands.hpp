#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_COMBATCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_COMBATCOMMANDS_H

#include <optional>

#include "combatstate.hpp"
#include "interpreter.hpp"

namespace FalloutScript
{
    class ActorState;

    /// What the commands about fighting ask of the game that CombatState and ActorState do not know
    class CombatWorld
    {
    public:
        virtual ~CombatWorld() = default;

        /// Whether the viewer notices the target (sees or hears it)
        virtual bool detects(FormId viewer, FormId target) = 0;
        /// Whether the record of the character makes it essential
        virtual bool essentialByRecord(FormId actor) = 0;
        /// Whether the mode of the game makes the character essential: a companion of the player cannot die unless the
        /// hardcore mode is on (FalloutNV.exe 0x87F3D0)
        virtual bool essentialByMode(FormId actor)
        {
            (void)actor;
            return false;
        }
        /// How the records of the game say two factions stand to each other; none when they say nothing
        virtual std::optional<CombatState::Relation> recordedRelation(FormId first, FormId second) = 0;
        /// Whether the faction counts the player as its enemy (the flag that a seen crime or SetPCEnemyofFaction sets) and
        /// the target is the player: the executables let that flag win over every relation (0x8B87A0)
        virtual bool playerEnemyOf(FormId /*faction*/, FormId /*target*/) { return false; }
    };

    /// How the actor stands to the target by the factions they are in, in what scripts said or else what the records
    /// say about each faction of the one toward each faction of the other: an ally when any pair says so, else a
    /// friend when any does, else an enemy when any does
    CombatState::Relation relationOfActors(
        CombatState& combat, ActorState& actors, CombatWorld& world, FormId actor, FormId target);

    /// How a faction stands to the target by the target's factions (the rule of relationOfActors, for a faction alone):
    /// what the media location controllers of New Vegas choose their music by
    CombatState::Relation relationOfFaction(
        CombatState& combat, ActorState& actors, CombatWorld& world, FormId faction, FormId target);

    /// Whether the actor can start a fight against the target: neither is dead and the actor is not out (unconscious)
    bool mayStartFight(const CombatState& combat, const ActorState& actors, FormId actor, FormId target);

    /// Starts the fight when mayStartFight says so, with no death or revival between the question and the start (and
    /// no character put out, which the combat state checks itself). True when the actor fights the target.
    bool startFight(CombatState& combat, ActorState& actors, FormId actor, FormId target);

    /// Whether the character does not die of damage: a script made it so (for the character or the form it was made
    /// from) or its record says so
    bool isEssential(CombatState& combat, ActorState& actors, CombatWorld& world, FormId actor);

    /// Gives the commands about fighting to the interpreter: StartCombat, StopCombat, StopCombatAlarmOnActor,
    /// IsInCombat, GetCombatTarget, IsCombatTarget, GetDetected, SetAlert, GetIsAlerted, GetAttacked, GetShouldAttack,
    /// SetUnconscious, GetUnconscious, SetRestrained, GetRestrained, SetEssential, SetActorRefEssential, IsEssential,
    /// IsActorRefEssential, SetEnemy, SetAlly, GetFactionCombatReaction, GetHitLocation, GetKillingBlowLimb,
    /// GetLastHitCritical, IsKiller, GetFriendHit, SetIgnoreFriendlyHits and GetIgnoreFriendlyHits. Those that the
    /// command table does not have are left out.
    void addCombatCommands(Interpreter& interpreter, CombatState& combat, ActorState& actors, CombatWorld& world);
}

#endif
