#include "luabindings.hpp"

#include <components/lua/asyncpackage.hpp>
#include <components/lua/utilpackage.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/datetimemanager.hpp"

#include "animationbindings.hpp"
#include "camerabindings.hpp"
#include "cellbindings.hpp"
#include "contentbindings.hpp"
#include "corebindings.hpp"
#include "debugbindings.hpp"
#include "inputbindings.hpp"
#include "localscripts.hpp"
#include "markupbindings.hpp"
#include "menuscripts.hpp"
#include "nearbybindings.hpp"
#include "objectbindings.hpp"
#include "postprocessingbindings.hpp"
#include "soundbindings.hpp"
#include "types/types.hpp"
#include "uibindings.hpp"
#include "vfsbindings.hpp"
#include "worldbindings.hpp"

namespace MWLua
{
    std::map<std::string, sol::object> initCommonPackages(const Context& context)
    {
        sol::state_view lua = context.mLua->unsafeState();
        MWWorld::DateTimeManager* tm = MWBase::Environment::get().getWorld()->getTimeManager();
        return {
            { "openfallout.async",
                LuaUtil::getAsyncPackageInitializer(
                    lua, [tm] { return tm->getSimulationTime(); }, [tm] { return tm->getGameTime(); }) },
            { "openfallout.markup", initMarkupPackage(context) },
            { "openfallout.util", LuaUtil::initUtilPackage(lua) },
            { "openfallout.vfs", initVFSPackage(context) },
        };
    }

    std::map<std::string, sol::object> initGlobalPackages(const Context& context)
    {
        initObjectBindingsForGlobalScripts(context);
        initCellBindingsForGlobalScripts(context);
        return {
            { "openfallout.core", initCorePackage(context) },
            { "openfallout.types", initTypesPackage(context) },
            { "openfallout.world", initWorldPackage(context) },
        };
    }

    std::map<std::string, sol::object> initLocalPackages(const Context& context)
    {
        initObjectBindingsForLocalScripts(context);
        initCellBindingsForLocalScripts(context);
        LocalScripts::initializeSelfPackage(context);
        return {
            { "openfallout.animation", initAnimationPackage(context) },
            { "openfallout.core", initCorePackage(context) },
            { "openfallout.types", initTypesPackage(context) },
            { "openfallout.nearby", initNearbyPackage(context) },
        };
    }

    std::map<std::string, sol::object> initPlayerPackages(const Context& context)
    {
        return {
            { "openfallout.ambient", initAmbientPackage(context) },
            { "openfallout.camera", initCameraPackage(context.sol()) },
            { "openfallout.debug", initDebugPackage(context) },
            { "openfallout.input", initInputPackage(context) },
            { "openfallout.postprocessing", initPostprocessingPackage(context) },
            { "openfallout.ui", initUserInterfacePackage(context) },
        };
    }

    std::map<std::string, sol::object> initMenuPackages(const Context& context)
    {
        return {
            { "openfallout.core", initCorePackage(context) },
            { "openfallout.ambient", initAmbientPackage(context) },
            { "openfallout.ui", initUserInterfacePackage(context) },
            { "openfallout.menu", initMenuPackage(context) },
            { "openfallout.input", initInputPackage(context) },
        };
    }

    std::map<std::string, sol::object> initLoadPackages(const Context& context)
    {
        return {
            { "openfallout.core", initCorePackage(context) },
            { "openfallout.content", initContentPackage(context) },
        };
    }
}
