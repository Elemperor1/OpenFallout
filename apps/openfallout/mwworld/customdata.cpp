#include "customdata.hpp"

#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace OFWorld
{

    OFClass::CreatureCustomData& CustomData::asCreatureCustomData()
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to CreatureCustomData";
        throw std::logic_error(error.str());
    }

    const OFClass::CreatureCustomData& CustomData::asCreatureCustomData() const
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to CreatureCustomData";
        throw std::logic_error(error.str());
    }

    OFClass::NpcCustomData& CustomData::asNpcCustomData()
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to NpcCustomData";
        throw std::logic_error(error.str());
    }

    const OFClass::NpcCustomData& CustomData::asNpcCustomData() const
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to NpcCustomData";
        throw std::logic_error(error.str());
    }

    OFClass::ContainerCustomData& CustomData::asContainerCustomData()
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to ContainerCustomData";
        throw std::logic_error(error.str());
    }

    const OFClass::ContainerCustomData& CustomData::asContainerCustomData() const
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to ContainerCustomData";
        throw std::logic_error(error.str());
    }

    OFClass::DoorCustomData& CustomData::asDoorCustomData()
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to DoorCustomData";
        throw std::logic_error(error.str());
    }

    const OFClass::DoorCustomData& CustomData::asDoorCustomData() const
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to DoorCustomData";
        throw std::logic_error(error.str());
    }

    OFClass::CreatureLevListCustomData& CustomData::asCreatureLevListCustomData()
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to CreatureLevListCustomData";
        throw std::logic_error(error.str());
    }

    const OFClass::CreatureLevListCustomData& CustomData::asCreatureLevListCustomData() const
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to CreatureLevListCustomData";
        throw std::logic_error(error.str());
    }

    OFClass::ESM4NpcCustomData& CustomData::asESM4NpcCustomData()
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to ESM4NpcCustomData";
        throw std::logic_error(error.str());
    }

    const OFClass::ESM4NpcCustomData& CustomData::asESM4NpcCustomData() const
    {
        std::stringstream error;
        error << "bad cast " << typeid(this).name() << " to ESM4NpcCustomData";
        throw std::logic_error(error.str());
    }
}
