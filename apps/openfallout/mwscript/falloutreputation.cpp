#include "falloutscripts.hpp"

#include <components/esm4/loadrepu.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }
    }

    bool FalloutScripts::reputation(FalloutScript::FormId reputation, double& cap, std::string& name)
    {
        const ESM4::Reputation* record = mStore.get<ESM4::Reputation>().search(refId(reputation));
        if (record == nullptr)
            return false;
        cap = record->mValue;
        name = record->mFullName;
        return true;
    }

    double FalloutScripts::setting(std::string_view name, double fallback)
    {
        return gameSetting(name, fallback);
    }
}
