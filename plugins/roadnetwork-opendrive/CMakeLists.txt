# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

project(RoadnetworkOpendrivePlugin
    VERSION 0.1.0
    DESCRIPTION "OpenUSD xodr file-format plugin"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenStrataPlugin.cmake")
openstrata_default_build_type()

# Resolved from the runtime OpenStrata activates (toolchain sets CMAKE_PREFIX_PATH).
# Guarded so a parent project (a dual-mode root that `add_subdirectory()`s this
# bundle) can resolve OpenUSD once and have it flow down here.
if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()

# OpenStrata builds and installs the libraries named in the manifest's
# requires.libraries before configuring this bundle. A composed root build
# already defines the targets.
if(NOT TARGET openstrata::roadnetworkCore)
    find_package(roadnetworkCore CONFIG REQUIRED)
endif()
if(NOT TARGET openstrata::roadnetworkUsd)
    find_package(roadnetworkUsd CONFIG REQUIRED)
endif()

set(PLUGIN_NAME RoadnetworkOpendriveFileFormat)

add_library(${PLUGIN_NAME} SHARED
    src/RoadnetworkOpendriveFileFormat.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE
    openstrata::roadnetworkCore
    openstrata::roadnetworkUsd)

# `ost plugin build` writes this fragment when the bundle co-hosts a compiled
# schema (`schema.usda` + `provides: usd-schema:*`). It adds the generated typed
# API sources to this same shared library and defines the generated `*_EXPORTS`
# macro so the schema symbols are exported from the plugin binary. Plain
# file-format bundles have no fragment and stay unchanged.
if(DEFINED OPENSTRATA_SCHEMA_SOURCES_FILE AND EXISTS "${OPENSTRATA_SCHEMA_SOURCES_FILE}")
    include("${OPENSTRATA_SCHEMA_SOURCES_FILE}")
endif()

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf usd)

# The copied helper keeps output/install mechanics identical in standalone and
# add_subdirectory workspace builds. This file still owns the target, OpenUSD
# components, resource path, and fixtures as the bundle's readable policy.
openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/roadnetwork-opendrive/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/roadnetwork-opendrive/plugInfo.json")

# Diagnostic catalogs (ADR 0006): this bundle's ODR- codes and a copy of the
# shared RN- codes from roadnetwork-core, installed side by side.
if(NOT roadnetworkCore_DIAGNOSTICS_CATALOG OR NOT EXISTS "${roadnetworkCore_DIAGNOSTICS_CATALOG}")
    message(FATAL_ERROR
        "roadnetwork-core did not provide its RN- diagnostic catalog "
        "(roadnetworkCore_DIAGNOSTICS_CATALOG='${roadnetworkCore_DIAGNOSTICS_CATALOG}').")
endif()
set(_rn_catalog "${CMAKE_CURRENT_BINARY_DIR}/openstrata/resources/rn-diagnostics.json")
configure_file("${roadnetworkCore_DIAGNOSTICS_CATALOG}" "${_rn_catalog}" COPYONLY)

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/roadnetwork-opendrive/plugInfo.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/roadnetwork-opendrive/diagnostics.json"
        "${_rn_catalog}"
    RESOURCE_DESTINATION "plugin/resources/roadnetwork-opendrive"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.scaffold.yaml"
    FIXTURE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

# File-format contract and catalog tests (ADR 0006). They run from the
# composed root build (`ost test`) and from a standalone configure.
if(NOT DEFINED ROADNETWORK_BUILD_TESTS)
    option(ROADNETWORK_BUILD_TESTS "Build roadnetwork-opendrive tests" ${PROJECT_IS_TOP_LEVEL})
endif()
if(ROADNETWORK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
