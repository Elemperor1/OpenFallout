#ifndef OPENFALLOUT_COMPONENTS_ESM4_WORNARMOR_H
#define OPENFALLOUT_COMPONENTS_ESM4_WORNARMOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "loadarmo.hpp"
#include "loadglob.hpp"
#include "loadlvli.hpp"
#include "loadlvln.hpp"
#include "loadnpc.hpp"

namespace ESM4
{
    // Which armour a character of Fallout 3 and New Vegas wears, from what its record, its templates and the levelled
    // lists say. The game and the counting tool (esmtool worn) both use this, so that what the tool counts on the real
    // files is what the game shows.

    // Where the records that an inventory names are found: the game answers from its store, the tool from the records
    // of the plugins it has read. An id that names no record of the kind gives a null pointer.
    class WornArmorSource
    {
    public:
        virtual ~WornArmorSource() = default;

        virtual const Npc* findNpc(ESM::FormId id) const = 0;
        virtual const LevelledNpc* findLevelledNpc(ESM::FormId id) const = 0;
        virtual const Armor* findArmor(ESM::FormId id) const = 0;
        virtual const LevelledItem* findLevelledItem(ESM::FormId id) const = 0;
        virtual const GlobalVariable* findGlobal(ESM::FormId id) const = 0;
    };

    // What happened while the armour was chosen, for counting. Only the totals of the whole census use it.
    struct WornArmorTrace
    {
        std::size_t mCharacters = 0;
        std::size_t mInventoryFromTemplate = 0; // the inventory is the one of a template
        std::size_t mNoInventory = 0; // no record of the template chain has an inventory of its own
        std::size_t mItems = 0; // the entries of the inventory
        std::size_t mArmorListed = 0; // entries that are a piece of armour
        std::size_t mListsEntered = 0; // entries that are a levelled list
        std::size_t mArmorFromLists = 0; // pieces of armour that levelled lists gave
        std::size_t mListsEmptyByChance = 0;
        std::size_t mListsWithGlobalChance = 0; // lists whose chance of nothing is the value of a global variable
        std::size_t mListsEmptyByLevel = 0; // no entry at or below the level of the character
        std::size_t mListsUsingAll = 0;
        std::size_t mListsTooDeep = 0;
        std::size_t mOtherItems = 0; // entries that name neither armour nor a list of items (weapons, ammunition, ...)
        std::size_t mTemplateListsEmpty = 0; // a levelled list of characters gave no character
    };

    // The level a character has: its own, or the level of the player times the multiplier that it has instead, between
    // the limits that it names. A record of another game than Fallout has the level of the player.
    int actorLevel(const Npc& npc, int playerLevel);

    // The records that a character gets traits, inventory and the rest from, the character itself first and then its
    // template, the template of that, and so on. A levelled list of characters in the place of a template gives one of
    // its characters, by the level of the record that names it. The seed is what chooses the character.
    std::vector<const Npc*> templateChain(const WornArmorSource& source, const Npc& npc, int playerLevel,
        std::uint32_t seed, WornArmorTrace* trace = nullptr);

    // The first record of the chain that does not take the thing the flag names (one of Npc::TemplateFlags) from the
    // next record, so that it is the record that has the thing. Null if every record of the chain takes it.
    const Npc* templateOwner(const std::vector<const Npc*>& chain, std::uint16_t flag);

    // The armour that the character wears, in the order of its inventory: the pieces its inventory lists, and those
    // that its levelled lists give for the level of the character. A list is chosen from with the seed, so that the
    // same character is always dressed the same and two characters of one list are not.
    std::vector<const Armor*> wornArmor(const WornArmorSource& source, const Npc& character, int playerLevel,
        std::uint32_t seed, WornArmorTrace* trace = nullptr);

    // A piece of armour that is worn, with the model it shows and the parts of the body it covers
    struct WornPiece
    {
        const Armor* mArmor = nullptr;
        const ESM::Path* mModel = nullptr;
        std::uint32_t mSlots = 0;
    };

    struct WornPieces
    {
        std::vector<WornPiece> mPieces;
        std::uint32_t mCovered = 0; // the parts of the body that the pieces cover
        std::size_t mWithoutModel = 0; // pieces left out for having no model
        std::size_t mWithoutSlots = 0; // pieces left out for covering no part of the body
        std::size_t mOverlapping = 0; // pieces left out for covering what a piece before them covers
    };

    // Which of the pieces show. A piece is worn if no piece before it covers any part of the body that it covers, so
    // that a character with two suits wears the first. A woman wears the female model of a piece, or the male one if
    // it has none.
    WornPieces wornPieces(const std::vector<const Armor*>& armor, bool isFemale);

    // The parts of the body that the biped flags of an armour name (the other bits are not parts of the body)
    constexpr std::uint32_t bipedSlotMask = 0x000FFFFF;
}

#endif
