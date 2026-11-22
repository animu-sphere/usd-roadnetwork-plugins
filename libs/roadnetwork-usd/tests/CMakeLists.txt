# SPDX-License-Identifier: Apache-2.0
add_executable(roadnetworkUsd_tests test_layer_writer.cpp)
target_link_libraries(roadnetworkUsd_tests PRIVATE openstrata::roadnetworkUsd)
if(MSVC)
    target_compile_definitions(roadnetworkUsd_tests PRIVATE NOMINMAX)
    target_compile_options(roadnetworkUsd_tests PRIVATE /utf-8)
endif()
add_test(NAME roadnetworkUsd_layer_writer COMMAND roadnetworkUsd_tests)
