#ifndef MWLUA_OBJECT_H
#define MWLUA_OBJECT_H

#include <stdexcept>

#include <sol/sol.hpp>

#include <components/esm3/cellref.hpp>

#include "../mwworld/ptr.hpp"

namespace OFLua
{
    // ObjectId is a unique identifier of a game object.
    // It can change only if the order of content files was change.
    using ObjectId = ESM::RefNum;
    inline ObjectId getId(const OFWorld::Ptr& ptr)
    {
        return ptr.getCellRef().getRefNum();
    }

    // Lua scripts can't use OFWorld::Ptr directly, because lifetime of a script can be longer than lifetime of Ptr.
    // `GObject` and `LObject` are intended to be passed to Lua as a userdata.
    // It automatically updates the underlying Ptr when needed.
    class Object : public OFWorld::SafePtr
    {
    public:
        using SafePtr::SafePtr;
        const OFWorld::Ptr& ptr() const
        {
            const OFWorld::Ptr& res = ptrOrEmpty();
            if (res.isEmpty())
                throw std::runtime_error("Object is not available: " + id().toString());
            return res;
        }

        virtual bool isLObject() const { return false; }
        virtual bool isGObject() const { return false; }
        virtual bool isSelfObject() const { return false; }
    };

    // Used only in local scripts
    struct LCell
    {
        OFWorld::CellStore* mStore;
    };
    class LObject : public Object
    {
        using Object::Object;

        bool isLObject() const override { return true; }
    };

    // Used only in global scripts
    struct GCell
    {
        OFWorld::CellStore* mStore;
    };
    class GObject : public Object
    {
        using Object::Object;

        bool isGObject() const override { return true; }
    };

    using ObjectIdList = std::shared_ptr<std::vector<ObjectId>>;
    template <typename Obj>
    struct ObjectList
    {
        ObjectIdList mIds;
    };
    using GObjectList = ObjectList<GObject>;
    using LObjectList = ObjectList<LObject>;

    template <typename Obj>
    struct Inventory
    {
        Obj mObj;
    };

    template <typename Obj>
    struct Owner
    {
        Obj mObj;
    };

    // Reuse userdata across equivalent pushes.
    int pushCachedObject(lua_State* state, const LObject& value);
    int pushCachedObject(lua_State* state, const GObject& value);
    int pushCachedObject(lua_State* state, const LCell& value);
    int pushCachedObject(lua_State* state, const GCell& value);

    // Forget world-bound cache entries.
    void clearObjectCaches(lua_State* state);
}

namespace sol
{
    namespace stack
    {
        template <>
        struct unqualified_pusher<OFLua::LObject>
        {
            static int push(lua_State* state, const OFLua::LObject& value)
            {
                return OFLua::pushCachedObject(state, value);
            }
        };
        template <>
        struct unqualified_pusher<OFLua::GObject>
        {
            static int push(lua_State* state, const OFLua::GObject& value)
            {
                return OFLua::pushCachedObject(state, value);
            }
        };
        template <>
        struct unqualified_pusher<OFLua::LCell>
        {
            static int push(lua_State* state, const OFLua::LCell& value)
            {
                return OFLua::pushCachedObject(state, value);
            }
        };
        template <>
        struct unqualified_pusher<OFLua::GCell>
        {
            static int push(lua_State* state, const OFLua::GCell& value)
            {
                return OFLua::pushCachedObject(state, value);
            }
        };
    }
}

#endif // MWLUA_OBJECT_H
