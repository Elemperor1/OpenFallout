set(MWINIIMPORT
    main.cpp
    importer.cpp
)

set(MWINIIMPORT_HEADER
    importer.hpp
)

source_group(launcher FILES ${MWINIIMPORT} ${MWINIIMPORT_HEADER})

openfallout_add_executable(openfallout-iniimporter
    ${MWINIIMPORT}
)

target_link_libraries(openfallout-iniimporter
    Boost::program_options
    components
)

if (WIN32)
    target_sources(openfallout-iniimporter PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()

if (NOT APPLE)
    INSTALL(TARGETS openfallout-iniimporter)
endif()

if (MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -municode")
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout-iniimporter PRIVATE --coverage)
    target_link_libraries(openfallout-iniimporter gcov)
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout-iniimporter REUSE_FROM components)
endif()
