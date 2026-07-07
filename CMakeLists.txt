cmake_minimum_required(VERSION 3.16)

project(imediaplayer LANGUAGES CXX)

# ---- Language standard ----------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Release build when no build type / multi-config generator is set.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---- Project options ------------------------------------------------------
option(IX_ENABLE_WARNINGS          "Enable a strong set of compiler warnings" ON)
option(IX_WARNINGS_AS_ERRORS       "Treat compiler warnings as errors"        OFF)
option(IX_ENABLE_HIDDEN_VISIBILITY "Hide symbols not explicitly exported"     OFF)
option(IX_BUILD_TESTS              "Build the unit tests"                     ON)

# ---- Symbol visibility ----------------------------------------------------
# When enabled, only symbols annotated with IX_*_EXPORT are exported from the
# shared libraries. These directory-scoped variables initialise the matching
# target properties for every target created afterwards (including
# subdirectories).
#
# NOTE: This is opt-in (default OFF) because the public API is not yet fully
# annotated with IX_CORE_EXPORT / IX_MULTIMEDIA_EXPORT. Enabling it today makes
# the shared libraries hide internal-but-referenced symbols (e.g. igettime,
# ix_open_pipe, iStringMatcher, iFreeListDefaultConstants::Sizes), which breaks
# linking of the tests and any external consumer. Turn ON only after every
# public symbol has the appropriate export macro.
if(IX_ENABLE_HIDDEN_VISIBILITY)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()

# ---- Shared warning flags (consumed via target_link_libraries) ------------
add_library(ix_project_warnings INTERFACE)
if(IX_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(ix_project_warnings INTERFACE /W4)
        if(IX_WARNINGS_AS_ERRORS)
            target_compile_options(ix_project_warnings INTERFACE /WX)
        endif()
    else()
        target_compile_options(ix_project_warnings INTERFACE
            -Wall
            -Wextra)
        if(IX_WARNINGS_AS_ERRORS)
            target_compile_options(ix_project_warnings INTERFACE -Werror)
        endif()
    endif()
endif()

# ---- Subprojects ----------------------------------------------------------
add_subdirectory(src/core)

include(FindPkgConfig)
pkg_check_modules(GST_PKG gstreamer-1.0)
if (DEFINED GST_PKG_INCLUDE_DIRS AND DEFINED GST_PKG_LDFLAGS)
    add_subdirectory(src/multimedia)
endif()

if(IX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
