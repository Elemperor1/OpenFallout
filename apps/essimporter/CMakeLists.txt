set(ESSIMPORTER_FILES
    main.cpp
    importer.cpp
    importplayer.cpp
    importnpcc.cpp
    importcrec.cpp
    importcellref.cpp
    importinventory.cpp
    importklst.cpp
    importcntc.cpp
    importgame.cpp
    importinfo.cpp
    importdial.cpp
    importques.cpp
    importjour.cpp
    importscri.cpp
    importscpt.cpp
    importproj.cpp
    importsplm.cpp
    converter.cpp
    convertacdt.cpp
    convertnpcc.cpp
    convertinventory.cpp
    convertcrec.cpp
    convertcntc.cpp
    convertscri.cpp
    convertscpt.cpp
    convertplayer.cpp
)

openmw_add_executable(openfallout-essimporter
    ${ESSIMPORTER_FILES}
)

target_link_libraries(openfallout-essimporter
    Boost::program_options
    components
)

if (WIN32)
    target_sources(openfallout-essimporter PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout-essimporter PRIVATE --coverage)
    target_link_libraries(openfallout-essimporter gcov)
endif()

if (NOT APPLE)
    INSTALL(TARGETS openfallout-essimporter)
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout-essimporter PRIVATE
        <algorithm>
        <filesystem>
        <fstream>
        <string>
        <vector>
    )
endif()
