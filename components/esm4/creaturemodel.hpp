#ifndef OPENFALLOUT_COMPONENTS_ESM4_CREATUREMODEL_H
#define OPENFALLOUT_COMPONENTS_ESM4_CREATUREMODEL_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm/formid.hpp>

#include "loadcrea.hpp"
#include "loadlvlc.hpp"

namespace ESM4
{
    // Which model a creature of Fallout 3 and New Vegas has, from what its record and its templates say. A creature
    // record names a skeleton (MODL), the models that hang on it (NIFZ, bare file names in the folder of the skeleton)
    // and the animation files that belong to it (KFFZ); 1,316 of the 2,235 creatures of New Vegas take all of that
    // from a template, which is a creature or a levelled list of creatures.

    // Where the records that a creature names are found
    class CreatureSource
    {
    public:
        virtual ~CreatureSource() = default;

        virtual const Creature* findCreature(ESM::FormId id) const = 0;
        virtual const LevelledCreature* findLevelledCreature(ESM::FormId id) const = 0;
    };

    // The level a creature has: its own, or the level of the player times the multiplier that it has instead, between
    // the limits that it names. A creature of another game than Fallout has the level of the player.
    int creatureLevel(const Creature& creature, int playerLevel);

    // The creature that a form id names, or that a levelled list of creatures in its place gives for the level. The
    // seed chooses among the entries of a list. Null when nothing is found (and for a list that gives nothing).
    const Creature* resolveCreature(const CreatureSource& source, ESM::FormId id, int level, std::uint32_t seed);

    // The records that a creature gets its model from, the creature itself first and then its template, the template
    // of that, and so on. A levelled list in the place of a template gives one of its creatures, by the level of the
    // record that names it.
    std::vector<const Creature*> creatureTemplateChain(
        const CreatureSource& source, const Creature& creature, int playerLevel, std::uint32_t seed);

    // What a creature shows: all paths are under meshes, as the records name them.
    struct CreatureModel
    {
        // The record that has the model: the first of the chain that does not take it from its template, else the first
        // that has a skeleton. Null when no record of the chain has one.
        const Creature* mOwner = nullptr;
        std::string mSkeleton;
        // The models that hang on the skeleton (NIFZ), each in the folder of the skeleton unless the name has a folder
        std::vector<std::string> mParts;
        // The animation files that the record lists (KFFZ), named like the parts
        std::vector<std::string> mAnimations;
    };

    CreatureModel creatureModel(
        const CreatureSource& source, const Creature& creature, int playerLevel, std::uint32_t seed);

    // The same for a chain of templates that is known already
    CreatureModel creatureModel(const std::vector<const Creature*>& chain);

    // The first record of the chain that does not take the thing the flag names (one of Creature::TemplateFlags) from
    // the next record, so that it is the record that has the thing. Null if every record of the chain takes it.
    const Creature* creatureTemplateOwner(const std::vector<const Creature*>& chain, std::uint16_t flag);

    // The file `name` in the folder of `skeleton`, unless the name has a folder of its own:
    // "creatures\\gecko\\skeleton.nif" and "gecko.nif" make "creatures/gecko/gecko.nif". Slashes are forward in the
    // result.
    std::string creatureFile(std::string_view skeleton, std::string_view name);
}

#endif
