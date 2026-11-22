# SPDX-License-Identifier: Apache-2.0
# Build conventions shared by the workspace libraries and the plugin bundles.
#
# Every member includes this file by a path relative to its own source
# directory, so it works the same in all three ways the repository is
# configured: the composed root, `ost library build` on one library, and
# `ost plugin build` on one bundle. It is a build-time helper only: nothing
# here is installed, and no exported package configuration refers to it.
#
# The bundle's cmake/OpenStrataPlugin.cmake is a pinned copy of the scaffold
# helper and stays unedited; this file holds only what the repository adds.
include_guard(GLOBAL)

# C++17 unless a toolchain (OpenStrata's sets 20) or the caller chose a
# standard. Each target also requests cxx_std_17 as a floor, so an installed
# consumer inherits it.
macro(roadnetwork_cxx_defaults)
    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 17)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endmacro()

# roadnetwork_tests_option(<var> <description>)
#
# Declares the option that decides whether a member builds its tests. Its
# default follows, in order: the composed root's ROADNETWORK_BUILD_TESTS, the
# BUILD_TESTING that `ost library build` sets for the selected library, then
# whether the member is the top-level project. An explicit -D<var>= wins over
# all of them.
macro(roadnetwork_tests_option var description)
    if(DEFINED ROADNETWORK_BUILD_TESTS)
        set(_rn_tests_default "${ROADNETWORK_BUILD_TESTS}")
    elseif(DEFINED BUILD_TESTING)
        set(_rn_tests_default "${BUILD_TESTING}")
    else()
        set(_rn_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(${var} "${description}" ${_rn_tests_default})
    unset(_rn_tests_default)
endmacro()

# roadnetwork_target_defaults(<target> [REPRODUCIBLE])
#
# MSVC settings every target gets: /utf-8 source decoding, and NOMINMAX so
# the <windows.h> that OpenUSD headers pull in defines no min/max macros.
#
# REPRODUCIBLE adds /Brepro to every tool that stamps wall-clock time into
# its output: cl.exe into object files, lib.exe into archive-member headers,
# and link.exe into the PE header and debug directory. Without it two builds
# of identical sources hash differently, and the release gate requires
# reproducible package digests (item 4). Use it on everything that ends up in
# a package; tests do not need it.
function(roadnetwork_target_defaults target)
    cmake_parse_arguments(ARG "REPRODUCIBLE" "" "" ${ARGN})
    if(NOT TARGET ${target})
        message(FATAL_ERROR "roadnetwork_target_defaults: '${target}' is not a target")
    endif()
    if(NOT MSVC)
        return()
    endif()

    target_compile_definitions(${target} PRIVATE NOMINMAX)
    target_compile_options(${target} PRIVATE /utf-8)
    if(NOT ARG_REPRODUCIBLE)
        return()
    endif()

    target_compile_options(${target} PRIVATE /Brepro)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "STATIC_LIBRARY")
        set_target_properties(${target} PROPERTIES STATIC_LIBRARY_OPTIONS /Brepro)
    elseif(_type MATCHES "^(SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE)$")
        target_link_options(${target} PRIVATE /Brepro)
    endif()
endfunction()

# roadnetwork_openusd_targets(<out-var> COMPONENTS <component>...)
#
# Sets <out-var> to the OpenUSD targets for the listed components. OpenUSD
# exports either a monolithic usd_ms target or one target per component, with
# or without a pxr:: namespace. This is the discovery that the scaffold's
# openstrata_link_openusd() does, returned rather than linked, so a library
# can link the result PUBLIC and keep the bundle helper out of its build.
function(roadnetwork_openusd_targets out_var)
    cmake_parse_arguments(ARG "" "" "COMPONENTS" ${ARGN})
    if(NOT ARG_COMPONENTS)
        message(FATAL_ERROR "roadnetwork_openusd_targets requires COMPONENTS")
    endif()

    set(_targets)
    foreach(_candidate usd_ms pxr::usd_ms)
        if(TARGET ${_candidate})
            set(${out_var} ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()
    foreach(_component IN LISTS ARG_COMPONENTS)
        if(TARGET ${_component})
            list(APPEND _targets ${_component})
        elseif(TARGET pxr::${_component})
            list(APPEND _targets pxr::${_component})
        endif()
    endforeach()
    if(NOT _targets)
        message(FATAL_ERROR
            "Could not locate OpenUSD CMake targets for: ${ARG_COMPONENTS}")
    endif()
    set(${out_var} ${_targets} PARENT_SCOPE)
endfunction()

# roadnetwork_openusd_test_environment(<out-var>)
#
# Sets <out-var> to ENVIRONMENT_MODIFICATION entries that put the OpenUSD
# found by find_package(pxr) on a test's PATH and PYTHONPATH. `ost test` runs
# CTest inside the activated runtime and needs none of this; a plain `ctest`
# does, because on Windows the OpenUSD DLLs resolve through PATH only.
# Empty when pxr was not found by its config file.
function(roadnetwork_openusd_test_environment out_var)
    set(_env)
    if(pxr_DIR)
        # An OpenUSD install and an OpenStrata runtime put pxrConfig.cmake at
        # the root; some SDKs put it under lib/cmake/pxr instead.
        get_filename_component(_usd_root "${pxr_DIR}" ABSOLUTE)
        if(NOT EXISTS "${_usd_root}/bin" AND NOT EXISTS "${_usd_root}/lib/python")
            get_filename_component(_usd_root "${pxr_DIR}/../../.." ABSOLUTE)
        endif()
        list(APPEND _env
            "PATH=path_list_prepend:${_usd_root}/bin"
            "PATH=path_list_prepend:${_usd_root}/lib"
            "PYTHONPATH=path_list_prepend:${_usd_root}/lib/python")
    endif()
    set(${out_var} "${_env}" PARENT_SCOPE)
endfunction()
