#include "manualref.hpp"
#include <components/esm/records.hpp>
#include <components/esm4/loadstat.hpp>

#include "esmstore.hpp"

namespace
{

    template <typename T>
    void create(const OFWorld::Store<T>& store, const ESM::RefId& name, std::any& refValue, OFWorld::Ptr& ptrValue)
    {
        const T* base = store.find(name);

        ESM::CellRef cellRef;
        cellRef.blank();
        cellRef.mRefID = name;

        refValue = OFWorld::LiveCellRef<T>(cellRef, base);
        ptrValue = OFWorld::Ptr(&std::any_cast<OFWorld::LiveCellRef<T>&>(refValue), nullptr);
    }

    template <typename T>
    void create(
        const OFWorld::Store<T>& /*store*/, const OFWorld::Ptr& templatePtr, std::any& refValue, OFWorld::Ptr& ptrValue)
    {
        refValue = *static_cast<OFWorld::LiveCellRef<T>*>(templatePtr.getBase());
        ptrValue = OFWorld::Ptr(&std::any_cast<OFWorld::LiveCellRef<T>&>(refValue), nullptr);
    }

    template <typename F>
    void visitRefStore(const OFWorld::ESMStore& store, ESM::RefId name, F func)
    {
        switch (store.find(name))
        {
            case ESM::REC_ACTI:
                return func(store.get<ESM::Activator>());
            case ESM::REC_ALCH:
                return func(store.get<ESM::Potion>());
            case ESM::REC_APPA:
                return func(store.get<ESM::Apparatus>());
            case ESM::REC_ARMO:
                return func(store.get<ESM::Armor>());
            case ESM::REC_BOOK:
                return func(store.get<ESM::Book>());
            case ESM::REC_CLOT:
                return func(store.get<ESM::Clothing>());
            case ESM::REC_CONT:
                return func(store.get<ESM::Container>());
            case ESM::REC_CREA:
                return func(store.get<ESM::Creature>());
            case ESM::REC_DOOR:
                return func(store.get<ESM::Door>());
            case ESM::REC_INGR:
                return func(store.get<ESM::Ingredient>());
            case ESM::REC_LEVC:
                return func(store.get<ESM::CreatureLevList>());
            case ESM::REC_LEVI:
                return func(store.get<ESM::ItemLevList>());
            case ESM::REC_LIGH:
                return func(store.get<ESM::Light>());
            case ESM::REC_LOCK:
                return func(store.get<ESM::Lockpick>());
            case ESM::REC_MISC:
                return func(store.get<ESM::Miscellaneous>());
            case ESM::REC_NPC_:
                return func(store.get<ESM::NPC>());
            case ESM::REC_PROB:
                return func(store.get<ESM::Probe>());
            case ESM::REC_REPA:
                return func(store.get<ESM::Repair>());
            case ESM::REC_STAT:
                return func(store.get<ESM::Static>());
            case ESM::REC_WEAP:
                return func(store.get<ESM::Weapon>());
            case ESM::REC_BODY:
                return func(store.get<ESM::BodyPart>());
            case ESM::REC_STAT4:
                return func(store.get<ESM4::Static>());
            case ESM::REC_TERM4:
                return func(store.get<ESM4::Terminal>());
            // The rest of the kinds of object that the cells of Fallout hold (PlaceAtMe makes them)
            case ESM::REC_ACTI4:
                return func(store.get<ESM4::Activator>());
            case ESM::REC_ALCH4:
                return func(store.get<ESM4::Potion>());
            case ESM::REC_AMMO4:
                return func(store.get<ESM4::Ammunition>());
            case ESM::REC_ARMO4:
                return func(store.get<ESM4::Armor>());
            case ESM::REC_BOOK4:
                return func(store.get<ESM4::Book>());
            case ESM::REC_CLOT4:
                return func(store.get<ESM4::Clothing>());
            case ESM::REC_CONT4:
                return func(store.get<ESM4::Container>());
            case ESM::REC_CREA4:
                return func(store.get<ESM4::Creature>());
            case ESM::REC_DOOR4:
                return func(store.get<ESM4::Door>());
            case ESM::REC_FLOR4:
                return func(store.get<ESM4::Flora>());
            case ESM::REC_FURN4:
                return func(store.get<ESM4::Furniture>());
            case ESM::REC_IMOD4:
                return func(store.get<ESM4::ItemMod>());
            case ESM::REC_INGR4:
                return func(store.get<ESM4::Ingredient>());
            case ESM::REC_LIGH4:
                return func(store.get<ESM4::Light>());
            case ESM::REC_MISC4:
                return func(store.get<ESM4::MiscItem>());
            case ESM::REC_MSTT4:
                return func(store.get<ESM4::MovableStatic>());
            case ESM::REC_NPC_4:
                return func(store.get<ESM4::Npc>());
            case ESM::REC_SCOL4:
                return func(store.get<ESM4::StaticCollection>());
            case ESM::REC_TREE4:
                return func(store.get<ESM4::Tree>());
            case ESM::REC_WEAP4:
                return func(store.get<ESM4::Weapon>());
            case 0:
                throw std::logic_error(
                    "failed to create manual cell ref for " + name.toDebugString() + " (unknown ID)");

            default:
                throw std::logic_error(
                    "failed to create manual cell ref for " + name.toDebugString() + " (unknown type)");
        }
    }
}

OFWorld::ManualRef::ManualRef(const OFWorld::ESMStore& store, const ESM::RefId& name, const int count)
{
    auto cb = [&](const auto& typedStore) { create(typedStore, name, mRef, mPtr); };
    visitRefStore(store, name, cb);

    mPtr.getCellRef().setCount(count);
}

OFWorld::ManualRef::ManualRef(const ESMStore& store, const Ptr& templatePtr, const int count)
{
    auto cb = [&](const auto& typedStore) { create(typedStore, templatePtr, mRef, mPtr); };
    visitRefStore(store, templatePtr.getCellRef().getRefId(), cb);

    mPtr.getCellRef().setCount(count);
    mPtr.getCellRef().unsetRefNum();
    mPtr.getRefData().setLuaScripts(nullptr);
}
