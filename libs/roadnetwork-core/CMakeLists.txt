# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.23)

project(roadnetworkCore
    VERSION 0.1.0
    DESCRIPTION "Format-independent road network model shared by the format bundles"
    LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/RoadnetworkBuild.cmake")
roadnetwork_cxx_defaults()
roadnetwork_reproducible_archives()
roadnetwork_tests_option(ROADNETWORKCORE_BUILD_TESTS "Build roadnetworkCore tests")

add_library(roadnetworkCore STATIC
    src/Geometry.cpp
    src/RoadNetworkData.cpp
    src/Tessellation.cpp
    src/Validation.cpp
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
roadnetwork_target_defaults(roadnetworkCore REPRODUCIBLE)

include(GNUInstallDirs)
install(TARGETS roadnetworkCore
    EXPORT roadnetworkCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# The RN- diagnostic catalog (ADR 0006). Every bundle ships a copy beside its
# own catalog. A composed root build reads it from the source tree; an
# installed consumer gets the same variable from roadnetworkCoreConfig.cmake.
set(ROADNETWORK_CORE_CATALOG_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/roadnetworkCore")
install(FILES resources/diagnostics.json
    DESTINATION "${ROADNETWORK_CORE_CATALOG_INSTALL_DIR}")
set(roadnetworkCore_DIAGNOSTICS_CATALOG
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/diagnostics.json"
    CACHE INTERNAL "RN- diagnostic catalog shipped with every bundle")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/roadnetworkCoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkCoreConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkCore"
    PATH_VARS ROADNETWORK_CORE_CATALOG_INSTALL_DIR)
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
