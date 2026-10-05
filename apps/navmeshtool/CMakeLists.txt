set(NAVMESHTOOL_LIB
    worldspacedata.cpp
    navmesh.cpp
)

source_group(apps\\navmeshtool FILES ${NAVMESHTOOL_LIB} main.cpp)

add_library(openfallout-navmeshtool-lib STATIC ${NAVMESHTOOL_LIB})

if (ANDROID)
    add_library(openfallout-navmeshtool SHARED main.cpp)
else()
    openfallout_add_executable(openfallout-navmeshtool main.cpp)
endif()

target_link_libraries(openfallout-navmeshtool openfallout-navmeshtool-lib)

target_link_libraries(openfallout-navmeshtool-lib
    Boost::program_options
    components
)

if (WIN32)
    target_sources(openfallout-navmeshtool PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout-navmeshtool PRIVATE --coverage)
    target_link_libraries(openfallout-navmeshtool gcov)
endif()

if (NOT APPLE)
    install(TARGETS openfallout-navmeshtool)
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout-navmeshtool REUSE_FROM components)
endif()
