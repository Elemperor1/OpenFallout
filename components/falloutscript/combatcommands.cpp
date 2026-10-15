#include "combatcommands.hpp"

#include <vector>

#include "actors.hpp"
#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        /// The number that GetFactionCombatReaction gives for how a faction stands to another
        double reactionNumber(std::optional<CombatState::Relation> relation)
        {
            if (!relation)
                return 0;
            switch (*relation)
            {
                case CombatState::Relation::Enemy:
                    return 1;
                case CombatState::Relation::Ally:
                    return 2;
                case CombatState::Relation::Friend:
                    return 3;
                case CombatState::Relation::Neutral:
                    break;
            }
            return 0;
        }

        /// The flag that follows the form in SetEnemy, SetAlly and SetEssential: set when the script gives none
        bool flagOrSet(const CallContext& call, std::size_t index)
        {
            return index >= call.mArguments.size() || intArgument(call, index) != 0;
        }

        /// Whether an optional flag of SetEnemy or SetAlly is given and set (1 makes the standing milder, 0 or none
        /// keeps it)
        bool flagGiven(const CallContext& call, std::size_t index)
        {
            return index < call.mArguments.size() && intArgument(call, index) != 0;
        }

        /// What a script said about two factions, else what the records say
        std::optional<CombatState::Relation> factionRelation(
            CombatState& combat, CombatWorld& world, FormId first, FormId second)
        {
            if (const std::optional<CombatState::Relation> said = combat.relation(first, second))
                return said;
            return world.recordedRelation(first, second);
        }
    }

    CombatState::Relation relationOfActors(
        CombatState& combat, ActorState& actors, CombatWorld& world, FormId actor, FormId target)
    {
        std::vector<std::pair<FormId, int>> mine;
        std::vector<std::pair<FormId, int>> theirs;
        if (!actors.factions(actor, mine) || !actors.factions(target, theirs))
            return CombatState::Relation::Neutral;
        bool ally = false;
        for (const auto& [first, firstRank] : mine)
            for (const auto& [second, secondRank] : theirs)
            {
                if (first == second)
                {
                    ally = true;
                    continue;
                }
                const std::optional<CombatState::Relation> relation = factionRelation(combat, world, first, second);
                if (relation == CombatState::Relation::Enemy)
                    return CombatState::Relation::Enemy;
                if (relation == CombatState::Relation::Ally || relation == CombatState::Relation::Friend)
                    ally = true;
            }
        return ally ? CombatState::Relation::Ally : CombatState::Relation::Neutral;
    }

    bool isEssential(CombatState& combat, ActorState& actors, CombatWorld& world, FormId actor)
    {
        if (actor == 0)
            return false;
        if (const std::optional<bool> said = combat.essential(actor))
            return *said;
        FormId base = 0;
        if (actors.baseOf(actor, base) && base != 0 && base != actor)
            if (const std::optional<bool> said = combat.essential(base))
                return *said;
        return world.essentialByRecord(actor);
    }

    void addCombatCommands(Interpreter& interpreter, CombatState& combat, ActorState& actors, CombatWorld& world)
    {
        interpreter.setHandler("StartCombat", [&combat](CallContext& call) {
            combat.start(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("StopCombat", [&combat](CallContext& call) {
            combat.stop(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("StopCombatAlarmOnActor", [&combat](CallContext& call) {
            // The alarm is over for the actor: it stops, and so does everyone who went for it
            combat.stop(call.mReference);
            for (const FormId attacker : combat.attackersOf(call.mReference))
                combat.stop(attacker);
            return 0.0;
        });
        interpreter.setHandler("IsInCombat", [&combat](CallContext& call) {
            return call.mReference != 0 && combat.inCombat(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetCombatTarget", [&combat](CallContext& call) {
            return static_cast<double>(call.mReference != 0 ? combat.target(call.mReference) : 0);
        });
        interpreter.setHandler("IsCombatTarget", [&combat](CallContext& call) {
            // The reference is what the other actor fights
            const FormId other = formArgument(call, 0);
            return call.mReference != 0 && other != 0 && combat.target(other) == call.mReference ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetDetected", [&world](CallContext& call) {
            const FormId target = formArgument(call, 0);
            return call.mReference != 0 && target != 0 && world.detects(call.mReference, target) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetAlert", [&combat](CallContext& call) {
            if (call.mReference != 0)
                combat.setAlert(call.mReference, intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetIsAlerted",
            [&combat](CallContext& call) { return call.mReference != 0 && combat.alert(call.mReference) ? 1.0 : 0.0; });
        interpreter.setHandler("GetAttacked", [&combat](CallContext& call) {
            return call.mReference != 0 && combat.attacked(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetShouldAttack", [&combat, &actors, &world](CallContext& call) {
            const FormId target = formArgument(call, 0);
            if (call.mReference == 0 || target == 0 || call.mReference == target || actors.isDead(target))
                return 0.0;
            if (combat.target(call.mReference) == target)
                return 1.0;
            return relationOfActors(combat, actors, world, call.mReference, target) == CombatState::Relation::Enemy
                ? 1.0
                : 0.0;
        });

        interpreter.setHandler("SetUnconscious", [&combat](CallContext& call) {
            if (call.mReference != 0)
                combat.setUnconscious(call.mReference, intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetUnconscious", [&combat](CallContext& call) {
            return call.mReference != 0 && combat.unconscious(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetRestrained", [&combat](CallContext& call) {
            if (call.mReference != 0)
                combat.setRestrained(call.mReference, intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetRestrained", [&combat](CallContext& call) {
            return call.mReference != 0 && combat.restrained(call.mReference) ? 1.0 : 0.0;
        });

        // SetEssential names the form that characters are made from, SetActorRefEssential the one character
        interpreter.setHandler("SetEssential", [&combat](CallContext& call) {
            const FormId form = formArgument(call, 0);
            if (form != 0)
                combat.setEssential(form, flagOrSet(call, 1));
            return 0.0;
        });
        interpreter.setHandler("SetActorRefEssential", [&combat](CallContext& call) {
            if (call.mReference != 0)
                combat.setEssential(call.mReference, flagOrSet(call, 0));
            return 0.0;
        });
        const auto essential = [&combat, &actors, &world](CallContext& call) {
            return isEssential(combat, actors, world, call.mReference) ? 1.0 : 0.0;
        };
        interpreter.setHandler("IsEssential", essential);
        interpreter.setHandler("IsActorRefEssential", essential);

        // The two flags that may follow the factions say how each stands to the other: the first how the first faction
        // stands to the second, the second the reverse. Enemy is 0 and neutral 1 for SetEnemy, ally is 0 and friend 1
        // for SetAlly; without flags both are 0.
        interpreter.setHandler("SetEnemy", [&combat](CallContext& call) {
            const FormId first = formArgument(call, 0);
            const FormId second = formArgument(call, 1);
            combat.setRelation(
                first, second, flagGiven(call, 2) ? CombatState::Relation::Neutral : CombatState::Relation::Enemy);
            combat.setRelation(
                second, first, flagGiven(call, 3) ? CombatState::Relation::Neutral : CombatState::Relation::Enemy);
            return 0.0;
        });
        interpreter.setHandler("SetAlly", [&combat](CallContext& call) {
            const FormId first = formArgument(call, 0);
            const FormId second = formArgument(call, 1);
            combat.setRelation(
                first, second, flagGiven(call, 2) ? CombatState::Relation::Friend : CombatState::Relation::Ally);
            combat.setRelation(
                second, first, flagGiven(call, 3) ? CombatState::Relation::Friend : CombatState::Relation::Ally);
            return 0.0;
        });
        interpreter.setHandler("GetFactionCombatReaction", [&combat, &world](CallContext& call) {
            const FormId first = formArgument(call, 0);
            const FormId second = formArgument(call, 1);
            if (first == 0 || second == 0)
                return 0.0;
            return reactionNumber(factionRelation(combat, world, first, second));
        });

        // What the last blow that the character took was like
        interpreter.setHandler("GetHitLocation", [&combat](CallContext& call) {
            CombatState::Hit hit;
            return call.mReference != 0 && combat.lastHit(call.mReference, hit) ? static_cast<double>(hit.mLimb) : -1.0;
        });
        interpreter.setHandler("GetKillingBlowLimb", [&combat, &actors](CallContext& call) {
            CombatState::Hit hit;
            return call.mReference != 0 && actors.isDead(call.mReference) && combat.lastHit(call.mReference, hit)
                ? static_cast<double>(hit.mLimb)
                : -1.0;
        });
        interpreter.setHandler("GetLastHitCritical", [&combat](CallContext& call) {
            CombatState::Hit hit;
            return call.mReference != 0 && combat.lastHit(call.mReference, hit) && hit.mCritical ? 1.0 : 0.0;
        });
        // The character was killed by the one that is named
        interpreter.setHandler("IsKiller", [&actors](CallContext& call) {
            const FormId killer = formArgument(call, 0);
            return call.mReference != 0 && killer != 0 && actors.killerOf(call.mReference) == killer ? 1.0 : 0.0;
        });
        // The last blow came from someone on its side
        interpreter.setHandler("GetFriendHit", [&combat, &actors, &world](CallContext& call) {
            CombatState::Hit hit;
            if (call.mReference == 0 || !combat.lastHit(call.mReference, hit) || hit.mAttacker == 0)
                return 0.0;
            return relationOfActors(combat, actors, world, call.mReference, hit.mAttacker)
                    == CombatState::Relation::Ally
                ? 1.0
                : 0.0;
        });
    }
}
