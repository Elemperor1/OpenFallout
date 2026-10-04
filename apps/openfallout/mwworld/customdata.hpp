#ifndef GAME_MWWORLD_CUSTOMDATA_H
#define GAME_MWWORLD_CUSTOMDATA_H

#include <memory>

namespace OFClass
{
    class CreatureCustomData;
    class ESM4NpcCustomData;
    class NpcCustomData;
    class ContainerCustomData;
    class DoorCustomData;
    class CreatureLevListCustomData;
}

namespace OFWorld
{
    /// \brief Base class for the MW-class-specific part of RefData
    class CustomData
    {
    public:
        virtual ~CustomData() {}

        virtual std::unique_ptr<CustomData> clone() const = 0;

        // Fast version of dynamic_cast<X&>. Needs to be overridden in the respective class.

        virtual OFClass::CreatureCustomData& asCreatureCustomData();
        virtual const OFClass::CreatureCustomData& asCreatureCustomData() const;

        virtual OFClass::NpcCustomData& asNpcCustomData();
        virtual const OFClass::NpcCustomData& asNpcCustomData() const;

        virtual OFClass::ContainerCustomData& asContainerCustomData();
        virtual const OFClass::ContainerCustomData& asContainerCustomData() const;

        virtual OFClass::DoorCustomData& asDoorCustomData();
        virtual const OFClass::DoorCustomData& asDoorCustomData() const;

        virtual OFClass::CreatureLevListCustomData& asCreatureLevListCustomData();
        virtual const OFClass::CreatureLevListCustomData& asCreatureLevListCustomData() const;

        virtual OFClass::ESM4NpcCustomData& asESM4NpcCustomData();
        virtual const OFClass::ESM4NpcCustomData& asESM4NpcCustomData() const;
    };

    template <class T>
    struct TypedCustomData : CustomData
    {
        std::unique_ptr<CustomData> clone() const final { return std::make_unique<T>(*static_cast<const T*>(this)); }
    };
}

#endif
