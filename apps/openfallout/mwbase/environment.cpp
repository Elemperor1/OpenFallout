#include "environment.hpp"

#include <cassert>

#include <components/resource/resourcesystem.hpp>

OFBase::Environment* OFBase::Environment::sThis = nullptr;

OFBase::Environment::Environment()
{
    assert(sThis == nullptr);
    sThis = this;
}

OFBase::Environment::~Environment()
{
    sThis = nullptr;
}
