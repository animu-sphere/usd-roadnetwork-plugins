# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.23)

project(roadnetworkUsd
    VERSION 0.1.0
    DESCRIPTION "Shared RoadNetworkData to OpenUSD layer authoring"
    LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/RoadnetworkBuild.cmake")
roadnetwork_cxx_defaults()
roadnetwork_tests_option(ROADNETWORKUSD_BUILD_TESTS "Build roadnetworkUsd tests")

# Resolved once by a composed root build; found here when this library is
# configured standalone.
if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()
if(NOT TARGET openstrata::roadnetworkCore)
    find_package(roadnetworkCore CONFIG REQUIRED)
endif()

add_library(roadnetworkUsd STATIC
    src/LayerWriter.cpp
    src/PrimNames.cpp)
add_library(openstrata::roadnetworkUsd ALIAS roadnetworkUsd)
set_target_properties(roadnetworkUsd PROPERTIES
    EXPORT_NAME roadnetworkUsd
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(roadnetworkUsd PUBLIC cxx_std_17)
target_include_directories(roadnetworkUsd
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(roadnetworkUsd PUBLIC openstrata::roadnetworkCore)

# The component list deliberately stops at sdf (ADR 0006): the writer authors
# Sdf specs only. Authoring through a UsdStage fails inside the SdfChangeBlock
# that SdfLayer::Reload opens around Read() on OpenUSD 26.08, which is the
# problem the usd-3dgs-plugins worker thread works around.
roadnetwork_openusd_targets(_rn_usd_pxr_targets COMPONENTS arch tf vt sdf)
target_link_libraries(roadnetworkUsd PUBLIC ${_rn_usd_pxr_targets})
# Build-tree only. An installed consumer gets OpenUSD's headers from the pxr
# targets linked above and from its own find_package(pxr), so the export must
# not freeze the build machine's runtime path.
target_include_directories(roadnetworkUsd PUBLIC
    "$<BUILD_INTERFACE:${PXR_INCLUDE_DIRS}>")

roadnetwork_target_defaults(roadnetworkUsd REPRODUCIBLE)

include(GNUInstallDirs)
install(TARGETS roadnetworkUsd
    EXPORT roadnetworkUsdTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/roadnetworkUsdConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkUsdConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkUsd")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkUsdConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT roadnetworkUsdTargets
    FILE roadnetworkUsdTargets.cmake
    NAMESPACE openstrata::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkUsd")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkUsdConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/roadnetworkUsdConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/roadnetworkUsd")

if(ROADNETWORKUSD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
