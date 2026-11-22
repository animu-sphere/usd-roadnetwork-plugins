# SPDX-License-Identifier: Apache-2.0
add_executable(roadnetworkUsd_tests test_layer_writer.cpp)
target_link_libraries(roadnetworkUsd_tests PRIVATE openstrata::roadnetworkUsd)
roadnetwork_target_defaults(roadnetworkUsd_tests)
add_test(NAME roadnetworkUsd_layer_writer COMMAND roadnetworkUsd_tests)

# The test executable loads the OpenUSD shared libraries; a plain `ctest`
# finds them through this environment.
roadnetwork_openusd_test_environment(_env)
if(_env)
    set_tests_properties(roadnetworkUsd_layer_writer PROPERTIES
        ENVIRONMENT_MODIFICATION "${_env}")
endif()
