# SPDX-License-Identifier: Apache-2.0
# The reader and decoder are USD-free, so their unit tests need neither
# OpenUSD nor Python.
add_executable(roadnetworkOpendrive_reader_tests test_reader.cpp)
target_link_libraries(roadnetworkOpendrive_reader_tests PRIVATE roadnetworkOpendriveIo)
roadnetwork_target_defaults(roadnetworkOpendrive_reader_tests)
add_test(NAME roadnetworkOpendrive_reader COMMAND roadnetworkOpendrive_reader_tests)

# The decoder test reads the fixtures from the source tree and runs
# validation from roadnetwork-core; it needs no OpenUSD either.
add_executable(roadnetworkOpendrive_decoder_tests test_decoder.cpp)
target_link_libraries(roadnetworkOpendrive_decoder_tests PRIVATE roadnetworkOpendriveIo)
target_compile_definitions(roadnetworkOpendrive_decoder_tests PRIVATE
    ROADNETWORK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
roadnetwork_target_defaults(roadnetworkOpendrive_decoder_tests)
add_test(NAME roadnetworkOpendrive_decoder COMMAND roadnetworkOpendrive_decoder_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_Interpreter_FOUND)
    message(STATUS "roadnetwork-opendrive: Python3 not found; skipping the contract tests")
    return()
endif()

get_filename_component(_bundle_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(_repo_root "${_bundle_root}/../.." ABSOLUTE)

# The catalog cross-check reads sources and JSON only; it needs no OpenUSD.
add_test(
    NAME roadnetworkOpendrive_diagnostics_catalog
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_diagnostics_catalog.py"
        --repo-root "${_repo_root}")

# The contract test loads this bundle from its source tree: the library built
# into lib/ and the plugInfo.json beside it.
roadnetwork_openusd_test_environment(_env)
list(APPEND _env
    "PATH=path_list_prepend:${_bundle_root}/lib"
    "PXR_PLUGINPATH_NAME=path_list_prepend:${_bundle_root}/plugin/resources/roadnetwork-opendrive")

add_test(
    NAME roadnetworkOpendrive_file_format_contract
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_file_format_contract.py")
set_tests_properties(roadnetworkOpendrive_file_format_contract PROPERTIES
    ENVIRONMENT_MODIFICATION "${_env}")
