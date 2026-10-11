#ifndef GAME_MWCLASS_ESM4SOURCES_H
#define GAME_MWCLASS_ESM4SOURCES_H

#include <components/esm4/creaturemodel.hpp>
#include <components/esm4/wornarmor.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFClass
{
    /// The records that the armour of a Fallout character is chosen from and its templates are followed through are
    /// the ones of the store
    class StoreWornArmorSource final : public ESM4::WornArmorSource
    {
    public:
        explicit StoreWornArmorSource(const OFWorld::ESMStore& store)
            : mStore(store)
        {
        }

        const ESM4::Npc* findNpc(ESM::FormId id) const override { return mStore.get<ESM4::Npc>().search(id); }
        const ESM4::LevelledNpc* findLevelledNpc(ESM::FormId id) const override
        {
            return mStore.get<ESM4::LevelledNpc>().search(id);
        }
        const ESM4::Armor* findArmor(ESM::FormId id) const override { return mStore.get<ESM4::Armor>().search(id); }
        const ESM4::LevelledItem* findLevelledItem(ESM::FormId id) const override
        {
            return mStore.get<ESM4::LevelledItem>().search(id);
        }
        const ESM4::GlobalVariable* findGlobal(ESM::FormId id) const override
        {
            return mStore.get<ESM4::GlobalVariable>().search(id);
        }

    private:
        const OFWorld::ESMStore& mStore;
    };

    /// The creatures of the store, for the templates of a creature
    class StoreCreatureSource final : public ESM4::CreatureSource
    {
    public:
        explicit StoreCreatureSource(const OFWorld::ESMStore& store)
            : mStore(store)
        {
        }

        const ESM4::Creature* findCreature(ESM::FormId id) const override
        {
            return mStore.get<ESM4::Creature>().search(id);
        }

        const ESM4::LevelledCreature* findLevelledCreature(ESM::FormId id) const override
        {
            return mStore.get<ESM4::LevelledCreature>().search(id);
        }

    private:
        const OFWorld::ESMStore& mStore;
    };
}

#endif
