set(BULLETMESHTOOL
    main.cpp
)
source_group(apps\\bulletobjecttool FILES ${BULLETMESHTOOL})

openfallout_add_executable(openfallout-bulletobjecttool ${BULLETMESHTOOL})

target_link_libraries(openfallout-bulletobjecttool
    Boost::program_options
    components
)

if (WIN32)
    target_sources(openfallout-bulletobjecttool PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout-bulletobjecttool PRIVATE --coverage)
    target_link_libraries(openfallout-bulletobjecttool gcov)
endif()

if (NOT APPLE)
    install(TARGETS openfallout-bulletobjecttool)
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout-bulletobjecttool REUSE_FROM components)
endif()
