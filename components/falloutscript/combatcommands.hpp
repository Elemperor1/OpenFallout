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
        /// How the records of the game say two factions stand to each other; none when they say nothing
        virtual std::optional<CombatState::Relation> recordedRelation(FormId first, FormId second) = 0;
    };

    /// How the actor stands to the target by the factions they are in: an enemy when a faction of the one is the enemy
    /// of a faction of the other, in what scripts said or else what the records say, an ally when none is an enemy and
    /// one is an ally
    CombatState::Relation relationOfActors(
        CombatState& combat, ActorState& actors, CombatWorld& world, FormId actor, FormId target);

    /// Whether the actor can start a fight against the target: neither is dead and the actor is not out (unconscious)
    bool mayStartFight(const CombatState& combat, const ActorState& actors, FormId actor, FormId target);

    /// Whether the character does not die of damage: a script made it so (for the character or the form it was made
    /// from) or its record says so
    bool isEssential(CombatState& combat, ActorState& actors, CombatWorld& world, FormId actor);

    /// Gives the commands about fighting to the interpreter: StartCombat, StopCombat, StopCombatAlarmOnActor,
    /// IsInCombat, GetCombatTarget, IsCombatTarget, GetDetected, SetAlert, GetIsAlerted, GetAttacked, GetShouldAttack,
    /// SetUnconscious, GetUnconscious, SetRestrained, GetRestrained, SetEssential, SetActorRefEssential, IsEssential,
    /// IsActorRefEssential, SetEnemy, SetAlly, GetFactionCombatReaction, GetHitLocation, GetKillingBlowLimb,
    /// GetLastHitCritical, IsKiller and GetFriendHit. Those that the command table does not have are left out.
    void addCombatCommands(Interpreter& interpreter, CombatState& combat, ActorState& actors, CombatWorld& world);
}

#endif
