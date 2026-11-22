# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

project(roadnetworkUsd
    VERSION 0.1.0
    DESCRIPTION "Shared RoadNetworkData to OpenUSD layer authoring"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Same precedence as roadnetworkCore: explicit option, composed root,
# `ost library build`'s BUILD_TESTING, then top-level.
if(NOT DEFINED ROADNETWORKUSD_BUILD_TESTS)
    if(DEFINED ROADNETWORK_BUILD_TESTS)
        set(_rn_usd_tests_default "${ROADNETWORK_BUILD_TESTS}")
    elseif(DEFINED BUILD_TESTING)
        set(_rn_usd_tests_default "${BUILD_TESTING}")
    else()
        set(_rn_usd_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(ROADNETWORKUSD_BUILD_TESTS "Build roadnetworkUsd tests"
           ${_rn_usd_tests_default})
endif()

# Resolved once by a composed root build; found here when this library is
# configured standalone.
if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()
if(NOT TARGET openstrata::roadnetworkCore)
    find_package(roadnetworkCore CONFIG REQUIRED)
endif()

add_library(roadnetworkUsd STATIC
    src/LayerWriter.cpp)
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

# OpenUSD exports either a monolithic usd_ms target or one target per
# component, with or without a pxr:: namespace. This mirrors the bundle's
# openstrata_link_openusd() without depending on a bundle's cmake/ directory.
set(_rn_usd_pxr_targets)
foreach(_candidate usd_ms pxr::usd_ms)
    if(TARGET ${_candidate})
        set(_rn_usd_pxr_targets ${_candidate})
        break()
    endif()
endforeach()
if(NOT _rn_usd_pxr_targets)
    foreach(_component arch tf vt sdf)
        if(TARGET ${_component})
            list(APPEND _rn_usd_pxr_targets ${_component})
        elseif(TARGET pxr::${_component})
            list(APPEND _rn_usd_pxr_targets pxr::${_component})
        endif()
    endforeach()
endif()
if(NOT _rn_usd_pxr_targets)
    message(FATAL_ERROR "roadnetworkUsd: could not locate OpenUSD CMake targets.")
endif()
target_link_libraries(roadnetworkUsd PUBLIC ${_rn_usd_pxr_targets})
# Build-tree only. An installed consumer gets OpenUSD's headers from the pxr
# targets linked above and from its own find_package(pxr), so the export must
# not freeze the build machine's runtime path.
target_include_directories(roadnetworkUsd PUBLIC
    "$<BUILD_INTERFACE:${PXR_INCLUDE_DIRS}>")

# /Brepro: see roadnetworkCore. NOMINMAX keeps <windows.h>, pulled in by
# OpenUSD headers, from defining min/max macros.
if(MSVC)
    target_compile_definitions(roadnetworkUsd PRIVATE NOMINMAX)
    target_compile_options(roadnetworkUsd PRIVATE /utf-8 /Brepro)
    set_target_properties(roadnetworkUsd PROPERTIES STATIC_LIBRARY_OPTIONS /Brepro)
endif()

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
