file(GLOB OPENCS_TESTS_SRC_FILES
    main.cpp
    model/world/testinfocollection.cpp
    model/world/testuniversalid.cpp
)

source_group(apps\\openfallout-cs-tests FILES ${OPENCS_TESTS_SRC_FILES})

openfallout_add_executable(openfallout-cs-tests ${OPENCS_TESTS_SRC_FILES})

target_include_directories(openfallout-cs-tests SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
target_include_directories(openfallout-cs-tests SYSTEM PRIVATE ${GMOCK_INCLUDE_DIRS})

target_link_libraries(openfallout-cs-tests PRIVATE
    openfallout-cs-lib
    GTest::GTest
    GMock::GMock
)

if (WIN32)
    target_sources(openfallout-cs-tests PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(openfallout-cs-tests PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout-cs-tests PRIVATE --coverage)
    target_link_libraries(openfallout-cs-tests PRIVATE gcov)
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout-cs-tests REUSE_FROM components)
endif()
