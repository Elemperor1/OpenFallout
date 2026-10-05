openmw_add_executable(openfallout_settings_access_benchmark access.cpp)
target_link_libraries(openfallout_settings_access_benchmark benchmark::benchmark components)

target_compile_definitions(openfallout_settings_access_benchmark
    PRIVATE OPENMW_PROJECT_SOURCE_DIR=u8"${PROJECT_SOURCE_DIR}")

if (UNIX AND NOT APPLE)
    target_link_libraries(openfallout_settings_access_benchmark ${CMAKE_THREAD_LIBS_INIT})
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout_settings_access_benchmark REUSE_FROM components)
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout_settings_access_benchmark PRIVATE --coverage)
    target_link_libraries(openfallout_settings_access_benchmark gcov)
endif()

if (WIN32)
    target_sources(openfallout_settings_access_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()
