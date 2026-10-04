#include "ref.hpp"

#include <components/interpreter/runtime.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "interpretercontext.hpp"

OFWorld::Ptr OFScript::ExplicitRef::operator()(Interpreter::Runtime& runtime, bool required, bool activeOnly) const
{
    ESM::RefId id = ESM::RefId::stringRefId(runtime.getStringLiteral(runtime[0].mInteger));
    runtime.pop();

    if (required)
        return OFBase::Environment::get().getWorld()->getPtr(id, activeOnly);
    else
        return OFBase::Environment::get().getWorld()->searchPtr(id, activeOnly);
}

OFWorld::Ptr OFScript::ImplicitRef::operator()(Interpreter::Runtime& runtime, bool required, bool activeOnly) const
{
    OFScript::InterpreterContext& context = static_cast<OFScript::InterpreterContext&>(runtime.getContext());

    return context.getReference(required);
}
