# SPDX-License-Identifier: Apache-2.0
add_executable(roadnetworkCore_tests test_core.cpp)
target_link_libraries(roadnetworkCore_tests PRIVATE openstrata::roadnetworkCore)
target_compile_definitions(roadnetworkCore_tests PRIVATE
    EXPECTED_CORE_VERSION="${PROJECT_VERSION}")
add_test(NAME roadnetworkCore_unit COMMAND roadnetworkCore_tests)

add_executable(roadnetworkCore_validation_tests test_validation.cpp)
target_link_libraries(roadnetworkCore_validation_tests PRIVATE openstrata::roadnetworkCore)
add_test(NAME roadnetworkCore_validation COMMAND roadnetworkCore_validation_tests)
