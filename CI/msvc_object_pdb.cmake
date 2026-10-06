# /Zi stores debug information outside objects, avoiding oversized LTO libraries.
# sccache can cache /Zi compilations only when each object has its own /Fd PDB.
# Loaded after project() so the MSVC platform compile rules are available.
if(MSVC AND CMAKE_CXX_COMPILER_LAUNCHER STREQUAL "sccache")
    foreach(language C CXX)
        if(NOT CMAKE_${language}_COMPILE_OBJECT MATCHES "<TARGET_COMPILE_PDB>")
            message(FATAL_ERROR "Unexpected MSVC ${language} compile rule; cannot assign per-object PDBs")
        endif()
        string(REPLACE "<TARGET_COMPILE_PDB>" "<OBJECT>.pdb"
            CMAKE_${language}_COMPILE_OBJECT "${CMAKE_${language}_COMPILE_OBJECT}")
    endforeach()
endif()
