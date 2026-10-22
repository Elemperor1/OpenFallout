#include "falloutscripts.hpp"

#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        /// The number a script knows a form by, 0 for the null form and for one that scripts can not name
        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }
    }

    bool FalloutScripts::isActor(FalloutScript::FormId reference)
    {
        if (reference == mPlayer)
            return true;
        ESM::FormId base;
        if (!baseOf(reference, base) || base.isZeroOrUnset())
            return false;
        return mStore.get<ESM4::Npc>().search(base) != nullptr || mStore.get<ESM4::Creature>().search(base) != nullptr;
    }

    FalloutScript::FormId FalloutScripts::playerBase()
    {
        ESM::FormId base;
        return baseOf(mPlayer, base) ? numberOf(base) : 0;
    }

    FalloutScript::FormId FalloutScripts::ownerForm(FalloutScript::FormId form)
    {
        // A script that names a character by its reference means the character
        const ESM::FormId id = ESM::FormId::fromUint32(form);
        ESM::FormId base;
        if (!id.isZeroOrUnset()
            && (form == mPlayer || mStore.get<ESM4::ActorCharacter>().search(id) != nullptr
                || mStore.get<ESM4::ActorCreature>().search(id) != nullptr)
            && baseOf(form, base) && !base.isZeroOrUnset())
            return numberOf(base);
        return form;
    }

    FalloutScript::FormId FalloutScripts::recordOwner(FalloutScript::FormId reference)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(reference);
        if (id.isZeroOrUnset())
            return 0;
        if (const ESM4::Reference* record = mStore.get<ESM4::Reference>().search(id))
            return numberOf(record->mOwner);
        if (const ESM4::ActorCharacter* record = mStore.get<ESM4::ActorCharacter>().search(id))
            return numberOf(record->mOwner);
        if (const ESM4::ActorCreature* record = mStore.get<ESM4::ActorCreature>().search(id))
            return numberOf(record->mOwner);
        return 0;
    }

    FalloutScript::FormId FalloutScripts::recordCellOwner(FalloutScript::FormId cell)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(cell);
        const ESM4::Cell* record = id.isZeroOrUnset() ? nullptr : mStore.get<ESM4::Cell>().search(id);
        return record != nullptr ? numberOf(record->mOwner) : 0;
    }

    bool FalloutScripts::recordCellPublic(FalloutScript::FormId cell)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(cell);
        const ESM4::Cell* record = id.isZeroOrUnset() ? nullptr : mStore.get<ESM4::Cell>().search(id);
        return record != nullptr && (record->mCellFlags & ESM4::CELL_Public) != 0;
    }
}
