# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

project(roadnetworkCore
    VERSION 0.1.0
    DESCRIPTION "Format-independent road network model shared by the format bundles"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tests follow, in order: an explicit ROADNETWORKCORE_BUILD_TESTS, the composed
# root's ROADNETWORK_BUILD_TESTS, the BUILD_TESTING that `ost library build`
# sets for the selected library, then whether this is the top-level project.
if(NOT DEFINED ROADNETWORKCORE_BUILD_TESTS)
    if(DEFINED ROADNETWORK_BUILD_TESTS)
        set(_rn_core_tests_default "${ROADNETWORK_BUILD_TESTS}")
    elseif(DEFINED BUILD_TESTING)
        set(_rn_core_tests_default "${BUILD_TESTING}")
    else()
        set(_rn_core_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(ROADNETWORKCORE_BUILD_TESTS "Build roadnetworkCore tests"
           ${_rn_core_tests_default})
endif()

add_library(roadnetworkCore STATIC
    src/Version.cpp)
add_library(openstrata::roadnetworkCore ALIAS roadnetworkCore)
set_target_properties(roadnetworkCore PROPERTIES
    EXPORT_NAME roadnetworkCore
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(roadnetworkCore PUBLIC cxx_std_17)
target_include_directories(roadnetworkCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_compile_definitions(roadnetworkCore PRIVATE
    ROADNETWORK_CORE_VERSION="${PROJECT_VERSION}")

# /Brepro: MSVC stamps wall-clock times into object files and archive-member
# headers, so two builds of identical sources hash differently. The release
# gate requires reproducible package digests; lib.exe needs the flag
# separately from cl.exe.
if(MSVC)
    target_compile_options(roadnetworkCore PRIVATE /utf-8 /Brepro)
    set_target_properties(roadnetworkCore PROPERTIES STATIC_LIBRARY_OPTIONS /Brepro)
endif()

include(GNUInstallDirs)
install(TARGETS roadnetworkCore
    EXPORT roadnetworkCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/roadnetworkCoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkCoreConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkCore")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT roadnetworkCoreTargets
    FILE roadnetworkCoreTargets.cmake
    NAMESPACE openstrata::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkCore")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkCoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkCoreConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkCore")

if(ROADNETWORKCORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
