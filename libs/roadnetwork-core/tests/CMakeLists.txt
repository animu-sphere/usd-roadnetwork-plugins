# SPDX-License-Identifier: Apache-2.0
add_executable(roadnetworkCore_tests test_core.cpp)
target_link_libraries(roadnetworkCore_tests PRIVATE openstrata::roadnetworkCore)
target_compile_definitions(roadnetworkCore_tests PRIVATE
    EXPECTED_CORE_VERSION="${PROJECT_VERSION}")
add_test(NAME roadnetworkCore_unit COMMAND roadnetworkCore_tests)
