#include "creaturemodel.hpp"

#include <algorithm>

#include "levelled.hpp"

namespace ESM4
{
    namespace
    {
        // A chain of templates that is longer is a cycle or a mistake
        constexpr std::size_t maxTemplateDepth = 16;

        const Creature* findInSource(const CreatureSource& source, ESM::FormId start, int level, LevelledRandom& random)
        {
            return resolveLevelledRecord<Creature>([&source](ESM::FormId c) { return source.findCreature(c); },
                [&source](ESM::FormId c) { return source.findLevelledCreature(c); }, start, level, random);
        }

        bool hasSkeleton(const Creature& creature)
        {
            return !creature.mModel.empty();
        }
    }

    int creatureLevel(const Creature& creature, int playerLevel)
    {
        if (!creature.mIsFONV)
            return playerLevel;
        return levelFromConfig(creature.mBaseConfig.fo3, playerLevel);
    }

    const Creature* resolveCreature(const CreatureSource& source, ESM::FormId id, int level, std::uint32_t seed)
    {
        LevelledRandom random(seed ^ 0x2545F491ull);
        return findInSource(source, id, level, random);
    }

    std::vector<const Creature*> creatureTemplateChain(
        const CreatureSource& source, const Creature& creature, int playerLevel, std::uint32_t seed)
    {
        LevelledRandom random(seed ^ 0x5851F42Dull);
        std::vector<const Creature*> chain{ &creature };
        while (chain.size() < maxTemplateDepth)
        {
            const Creature& current = *chain.back();
            if (current.mBaseTemplate.isZeroOrUnset())
                break;
            // As for characters, a list in the place of a template is resolved for the level of the player: the level
            // of the record that names it is one it takes from its template
            const Creature* next = findInSource(source, current.mBaseTemplate, playerLevel, random);
            if (next == nullptr || std::find(chain.begin(), chain.end(), next) != chain.end())
                break;
            chain.push_back(next);
        }
        return chain;
    }

    const Creature* creatureTemplateOwner(const std::vector<const Creature*>& chain, std::uint16_t flag)
    {
        for (const Creature* record : chain)
            if (!record->takesFromTemplate(flag))
                return record;
        return nullptr;
    }

    CreatureModel creatureModel(
        const CreatureSource& source, const Creature& creature, int playerLevel, std::uint32_t seed)
    {
        return creatureModel(creatureTemplateChain(source, creature, playerLevel, seed));
    }

    CreatureModel creatureModel(const std::vector<const Creature*>& chain)
    {
        // The record that the flags leave the model with, else the first that has a skeleton
        const Creature* owner = creatureTemplateOwner(chain, Creature::Template_UseModel);
        if (owner == nullptr || !hasSkeleton(*owner))
        {
            const auto found
                = std::find_if(chain.begin(), chain.end(), [](const Creature* r) { return hasSkeleton(*r); });
            owner = found == chain.end() ? nullptr : *found;
        }

        CreatureModel model;
        model.mOwner = owner;
        if (owner == nullptr)
            return model;

        model.mSkeleton = creatureFile({}, owner->mModel.getOriginal());
        for (const std::string& part : owner->mNif)
            if (!part.empty())
                model.mParts.push_back(creatureFile(model.mSkeleton, part));
        for (const std::string& animation : owner->mKf)
            if (!animation.empty())
                model.mAnimations.push_back(creatureFile(model.mSkeleton, animation));
        return model;
    }

    std::string creatureFile(std::string_view skeleton, std::string_view name)
    {
        std::string file(name);
        std::replace(file.begin(), file.end(), '\\', '/');

        const bool hasFolder = file.find('/') != std::string::npos;
        if (!hasFolder)
        {
            std::string folder(skeleton);
            std::replace(folder.begin(), folder.end(), '\\', '/');
            const std::size_t slash = folder.find_last_of('/');
            if (slash != std::string::npos)
                file = folder.substr(0, slash + 1) + file;
        }
        return file;
    }
}
