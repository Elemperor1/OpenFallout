if (NOT DEFINED OPENFALLOUT_RESOURCES_ROOT)
    return()
endif()

set(LUA_API_FILES
    README.md
    coroutine.doclua
    global.doclua
    math.doclua
    string.doclua
    table.doclua
    openfallout/ambient.lua
    openfallout/animation.lua
    openfallout/async.lua
    openfallout/camera.lua
    openfallout/content.lua
    openfallout/core.lua
    openfallout/debug.lua
    openfallout/input.lua
    openfallout/interfaces.lua
    openfallout/markup.lua
    openfallout/menu.lua
    openfallout/nearby.lua
    openfallout/postprocessing.lua
    openfallout/self.lua
    openfallout/storage.lua
    openfallout/types.lua
    openfallout/ui.lua
    openfallout/util.lua
    openfallout/vfs.lua
    openfallout/world.lua
)

foreach (f ${LUA_API_FILES})
    copy_resource_file("${CMAKE_CURRENT_SOURCE_DIR}/${f}" "${OPENFALLOUT_RESOURCES_ROOT}" "resources/lua_api/${f}")
endforeach (f)
