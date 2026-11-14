# This script re-signs OpenFallout.app and OpenFallout-CS.app after CPack packages them. This is necessary because CPack modifies
# the library references used by OpenFallout to App relative paths, invalidating the code signature.
#
# Signing identity comes only from the environment:
#   OPENFALLOUT_CODESIGN_IDENTITY  Developer ID Application identity (name or SHA-1). When it is set, every nested
#                                  library, plug-in and helper executable is signed on its own, then the bundle
#                                  (inside out, no --deep). When it is unset or empty, the bundle gets today's ad hoc
#                                  signature. "-" takes the same hardened path with an ad hoc signature: it checks
#                                  that signing and `codesign --verify` work without a certificate, but the result
#                                  does not start (an ad hoc signature has no Team ID, so library validation refuses
#                                  the bundled libraries). To launch a hardened build locally, sign with an identity
#                                  that has a Team ID (e.g. an Apple Development certificate); CI/check_mac_package.py
#                                  reports the problem without starting the app.
#   OPENFALLOUT_CODESIGN_TEAM_ID   Team identifier. When set, each signed bundle must verify against a Developer ID
#                                  requirement for that team.
#   OPENFALLOUT_CODESIGN_LOCAL_TEST  LOCAL TEST ONLY, accepted only with identity "-": adds
#                                  files/mac/local-hardened-test.entitlements (library validation off) to each app's
#                                  entitlements, so that a hardened ad hoc package starts on a Mac without a
#                                  certificate. The JIT rules of the hardened runtime are the same; never ship it.
# Neither value is printed.
#
# Entitlements: CPACK_OPENFALLOUT_ENTITLEMENTS holds one entitlements file per entry of CPACK_OPENFALLOUT_APPLICATIONS
# (files/mac/*.entitlements). They are applied to the bundle, its helper executables and the apps nested in
# Contents/Helpers only; libraries and plug-ins take the entitlements of the process that loads them.

# Obviously, we only need to run this on Apple targets.
if (APPLE)
    cmake_policy(PUSH)
    # Do not walk into symlinked directories (osgPlugins-<version>, framework Versions/Current).
    cmake_policy(SET CMP0009 NEW)

    set(SIGNING_IDENTITY "$ENV{OPENFALLOUT_CODESIGN_IDENTITY}")
    set(SIGNING_TEAM_ID "$ENV{OPENFALLOUT_CODESIGN_TEAM_ID}")

    # Returns the code items nested in a bundle, deepest first: Mach-O files, with each framework signed as a whole.
    # The bundle's own main executable is left out; the bundle signature seals it.
    function(openfallout_nested_code bundle_path result_var)
        set(main_executable "")
        if (EXISTS "${bundle_path}/Contents/Info.plist")
            file(READ "${bundle_path}/Contents/Info.plist" info_plist)
            if (info_plist MATCHES "<key>CFBundleExecutable</key>[ \t\r\n]*<string>([^<]+)</string>")
                set(main_executable "${bundle_path}/Contents/MacOS/${CMAKE_MATCH_1}")
            endif()
        endif()

        file(GLOB_RECURSE candidates LIST_DIRECTORIES false "${bundle_path}/Contents/*")
        set(items "")
        foreach (candidate IN LISTS candidates)
            if (IS_SYMLINK "${candidate}" OR candidate STREQUAL main_executable)
                continue()
            endif()
            file(READ "${candidate}" magic LIMIT 4 HEX)
            if (NOT magic MATCHES "^(cffaedfe|cefaedfe|feedfacf|feedface|cafebabe|bebafeca)$")
                continue()
            endif()
            file(RELATIVE_PATH relative "${bundle_path}" "${candidate}")
            if (relative MATCHES "^(Contents/Helpers/[^/]+\\.app)/")
                # A nested app (the engine app) is signed as a bundle, which seals its own executable.
                set(candidate "${bundle_path}/${CMAKE_MATCH_1}")
            elseif (relative MATCHES "^(.*\\.framework)/")
                set(candidate "${bundle_path}/${CMAKE_MATCH_1}")
            endif()
            list(APPEND items "${candidate}")
        endforeach()
        list(REMOVE_DUPLICATES items)

        # Sort deepest first so anything nested inside another item is signed before it.
        set(keyed "")
        foreach (item IN LISTS items)
            string(REGEX REPLACE "[^/]" "" slashes "${item}")
            string(LENGTH "${slashes}" depth)
            math(EXPR key "1000 - ${depth}")
            list(APPEND keyed "${key}|${item}")
        endforeach()
        list(SORT keyed)
        set(sorted "")
        foreach (entry IN LISTS keyed)
            string(REGEX REPLACE "^[0-9]+\\|" "" entry "${entry}")
            list(APPEND sorted "${entry}")
        endforeach()
        set(${result_var} "${sorted}" PARENT_SCOPE)
    endfunction()

    set(LOCAL_TEST "$ENV{OPENFALLOUT_CODESIGN_LOCAL_TEST}")
    if (LOCAL_TEST AND NOT SIGNING_IDENTITY STREQUAL "-")
        message(FATAL_ERROR "OPENFALLOUT_CODESIGN_LOCAL_TEST turns library validation off and is for hardened ad hoc "
                            "test packages only (OPENFALLOUT_CODESIGN_IDENTITY=-); it is refused with any other identity")
    endif()

    # The app's entitlements plus the local-test overlay, written next to the staged apps (never into the source tree).
    function(openfallout_local_test_entitlements app_name release_file result_var)
        file(READ "${release_file}" release)
        # an app without entitlements has an empty dictionary (<dict/>): open it so the overlay has a place to go
        string(REPLACE "<dict/>" "<dict>\n</dict>" release "${release}")
        file(READ "${CMAKE_CURRENT_LIST_DIR}/../files/mac/local-hardened-test.entitlements" overlay)
        string(REGEX MATCH "<dict>(.*)</dict>" overlay_dict "${overlay}")
        string(FIND "${release}" "</dict>" dict_end REVERSE)
        string(SUBSTRING "${release}" 0 ${dict_end} head)
        string(SUBSTRING "${release}" ${dict_end} -1 tail)
        set(merged "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/${app_name}.local-test.entitlements")
        file(WRITE "${merged}" "${head}${CMAKE_MATCH_1}${tail}")
        set(${result_var} "${merged}" PARENT_SCOPE)
    endfunction()

    list(LENGTH CPACK_OPENFALLOUT_APPLICATIONS APPLICATION_COUNT)
    list(LENGTH CPACK_OPENFALLOUT_ENTITLEMENTS ENTITLEMENTS_COUNT)
    if (NOT SIGNING_IDENTITY STREQUAL "" AND NOT APPLICATION_COUNT EQUAL ENTITLEMENTS_COUNT)
        message(FATAL_ERROR "CPACK_OPENFALLOUT_ENTITLEMENTS needs one entitlements file per application")
    endif()

    foreach(app_name IN LISTS CPACK_OPENFALLOUT_APPLICATIONS)
        set(FULL_APP_PATH "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/ALL_IN_ONE/${app_name}")
        message(STATUS "Re-signing ${app_name}")
        # Apple's codesign utility does not like directories with periods (.) in their names, so we'll remove it and
        # create a symlink using the original name, which codesign is fine with.
        file(GLOB OSG_PLUGINS_DIR "${FULL_APP_PATH}/Contents/PlugIns/osgPlugins*")
        file(RENAME "${OSG_PLUGINS_DIR}" "${FULL_APP_PATH}/Contents/PlugIns/osgPlugins")
        execute_process(COMMAND "ln" "-s" "osgPlugins" "${OSG_PLUGINS_DIR}"
                        WORKING_DIRECTORY "${FULL_APP_PATH}/Contents/PlugIns/")

        if (SIGNING_IDENTITY STREQUAL "")
            execute_process(COMMAND "codesign" "--force" "--deep" "-s" "-" "${FULL_APP_PATH}")
            continue()
        endif()

        list(FIND CPACK_OPENFALLOUT_APPLICATIONS "${app_name}" APP_INDEX)
        list(GET CPACK_OPENFALLOUT_ENTITLEMENTS ${APP_INDEX} APP_ENTITLEMENTS)
        if (NOT EXISTS "${APP_ENTITLEMENTS}")
            message(FATAL_ERROR "Entitlements file for ${app_name} not found: ${APP_ENTITLEMENTS}")
        endif()
        if (LOCAL_TEST)
            message(STATUS "LOCAL TEST signature for ${app_name}: hardened ad hoc with library validation off")
            openfallout_local_test_entitlements("${app_name}" "${APP_ENTITLEMENTS}" APP_ENTITLEMENTS)
        endif()

        openfallout_nested_code("${FULL_APP_PATH}" NESTED_CODE)
        foreach (item IN LISTS NESTED_CODE ITEMS "${FULL_APP_PATH}")
            set(entitlement_args "")
            string(FIND "${item}" "${FULL_APP_PATH}/Contents/MacOS/" helper_position)
            if (item STREQUAL FULL_APP_PATH OR helper_position EQUAL 0 OR item MATCHES "/Contents/Helpers/[^/]+\\.app$")
                set(entitlement_args "--entitlements" "${APP_ENTITLEMENTS}")
            endif()
            execute_process(COMMAND "codesign" "--force" "--options" "runtime" "--timestamp" ${entitlement_args}
                                    "--sign" "${SIGNING_IDENTITY}" "${item}"
                            COMMAND_ERROR_IS_FATAL ANY)
        endforeach()

        set(requirement_args "")
        if (NOT SIGNING_TEAM_ID STREQUAL "")
            set(requirement_args "-R=anchor apple generic and certificate leaf[subject.OU] = \"${SIGNING_TEAM_ID}\"")
        endif()
        execute_process(COMMAND "codesign" "--verify" "--deep" "--strict" ${requirement_args} "${FULL_APP_PATH}"
                        COMMAND_ERROR_IS_FATAL ANY)
    endforeach(app_name)

    cmake_policy(POP)
endif (APPLE)
