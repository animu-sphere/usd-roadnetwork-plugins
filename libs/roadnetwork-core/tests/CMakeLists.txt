# SPDX-License-Identifier: Apache-2.0
# roadnetwork_add_core_test(<name> <source>): one executable and one CTest
# entry, named roadnetworkCore_<name>_tests and roadnetworkCore_<name>.
function(roadnetwork_add_core_test name source)
    set(_exe roadnetworkCore_${name}_tests)
    add_executable(${_exe} ${source})
    target_link_libraries(${_exe} PRIVATE openstrata::roadnetworkCore)
    roadnetwork_target_defaults(${_exe})
    add_test(NAME roadnetworkCore_${name} COMMAND ${_exe})
endfunction()

roadnetwork_add_core_test(unit test_core.cpp)
target_compile_definitions(roadnetworkCore_unit_tests PRIVATE
    EXPECTED_CORE_VERSION="${PROJECT_VERSION}")
roadnetwork_add_core_test(validation test_validation.cpp)
roadnetwork_add_core_test(geometry test_geometry.cpp)
roadnetwork_add_core_test(tessellation test_tessellation.cpp)
