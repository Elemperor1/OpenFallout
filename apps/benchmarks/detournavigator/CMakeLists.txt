openfallout_add_executable(openfallout_detournavigator_navmeshtilescache_benchmark navmeshtilescache.cpp)
target_link_libraries(openfallout_detournavigator_navmeshtilescache_benchmark benchmark::benchmark components)

if (UNIX AND NOT APPLE)
    target_link_libraries(openfallout_detournavigator_navmeshtilescache_benchmark ${CMAKE_THREAD_LIBS_INIT})
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openfallout_detournavigator_navmeshtilescache_benchmark REUSE_FROM components)
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openfallout_detournavigator_navmeshtilescache_benchmark PRIVATE --coverage)
    target_link_libraries(openfallout_detournavigator_navmeshtilescache_benchmark gcov)
endif()

if (WIN32)
    target_sources(openfallout_detournavigator_navmeshtilescache_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()
