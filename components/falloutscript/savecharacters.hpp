#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVECHARACTERS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVECHARACTERS_H

#include <cstdint>

#include "savestate.hpp"

namespace FalloutScript
{
    class ActorState;
    class ActorValues;
    class CombatState;
    class Equipment;
    class FormLists;
    class ItemUse;
    class PerkEngine;
    class PlayerProgress;
    class ReferenceState;

    /// The sections of a saved game that hold what scripts and the games' rules made of characters, creatures and
    /// containers: what they carry, wear and are, how far the player has come, and who fights whom. Reading puts the
    /// saved state on top of an empty one (after clear()); what it says about a form of a plugin that is gone is left
    /// out.

    /// The items and factions that scripts gave or took, who is dead and who killed them, and the deaths and item
    /// changes that the game has not raised the events of yet
    constexpr std::uint32_t actorsSectionVersion = 1;
    void writeActors(ActorState& state, SaveWriter& out);
    void readActors(ActorState& state, SaveReader& in, std::uint32_t version);

    /// What each character holds in its hands and wears
    constexpr std::uint32_t equipmentSectionVersion = 1;
    void writeEquipment(Equipment& equipment, SaveWriter& out);
    void readEquipment(Equipment& equipment, SaveReader& in, std::uint32_t version);

    /// The base, the modifier and the damage of every actor value that scripts or rules changed
    constexpr std::uint32_t actorValuesSectionVersion = 1;
    void writeActorValues(ActorValues& values, SaveWriter& out);
    void readActorValues(ActorValues& values, SaveReader& in, std::uint32_t version);

    /// Experience, karma, level-up points, the perks and notes of the characters, and the statistics
    constexpr std::uint32_t progressSectionVersion = 1;
    void writeProgress(PlayerProgress& progress, SaveWriter& out);
    void readProgress(PlayerProgress& progress, SaveReader& in, std::uint32_t version);

    /// What the abilities of perks have added to actor values at the moment (read after the actor values and the
    /// progress: the sum is in the actor values, which this must not add again)
    constexpr std::uint32_t perksSectionVersion = 1;
    void writePerks(PerkEngine& perks, ActorValues& values, SaveWriter& out);
    void readPerks(PerkEngine& perks, ActorValues& values, SaveReader& in, std::uint32_t version);

    /// The effects of chems and food that are running, and the addictions
    constexpr std::uint32_t itemUseSectionVersion = 1;
    void writeItemUse(ItemUse& use, SaveWriter& out);
    void readItemUse(ItemUse& use, SaveReader& in, std::uint32_t version);

    /// Who fights whom, who is unconscious or restrained, how factions stand to each other, the blows taken
    constexpr std::uint32_t combatSectionVersion = 1;
    void writeCombat(CombatState& combat, SaveWriter& out);
    void readCombat(CombatState& combat, SaveReader& in, std::uint32_t version);

    /// The locks that scripts set and the references that were destroyed, and the form lists that scripts changed
    constexpr std::uint32_t referencesStateSectionVersion = 1;
    void writeReferenceState(ReferenceState& state, SaveWriter& out);
    void readReferenceState(ReferenceState& state, SaveReader& in, std::uint32_t version);

    constexpr std::uint32_t formListsSectionVersion = 1;
    void writeFormLists(FormLists& lists, SaveWriter& out);
    void readFormLists(FormLists& lists, SaveReader& in, std::uint32_t version);
}

#endif
