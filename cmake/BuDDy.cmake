# Build BuDDy as an ordinary CMake target.
#
# Upstream ships an autotools build, but the sources barely depend on it: only
# kernel.c includes config.h, and the only macros it reads are the three
# version numbers. Generating those ourselves means no autoreconf/configure/
# libtool at build time, which keeps the first configure fast and works on any
# machine that has a C and C++ compiler.
#
# Set FTEC_BUDDY_SOURCE_DIR to build from a checkout you already have instead
# of fetching one (useful offline, or when iterating on BuDDy itself).

set(FTEC_BUDDY_SOURCE_DIR "" CACHE PATH
    "Existing BuDDy checkout to build from; fetched automatically when empty")
set(FTEC_BUDDY_MAJOR 2)
set(FTEC_BUDDY_MINOR 4)

if(FTEC_BUDDY_SOURCE_DIR)
    if(NOT EXISTS "${FTEC_BUDDY_SOURCE_DIR}/src/bdd.h")
        message(FATAL_ERROR
            "FTEC_BUDDY_SOURCE_DIR=${FTEC_BUDDY_SOURCE_DIR} does not look like a "
            "BuDDy checkout (no src/bdd.h)")
    endif()
    set(_buddy_src "${FTEC_BUDDY_SOURCE_DIR}")
    message(STATUS "BuDDy: using ${_buddy_src}")
else()
    include(FetchContent)
    FetchContent_Declare(buddy
        GIT_REPOSITORY https://github.com/utwente-fmt/buddy.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE)
    # BuDDy has no CMakeLists.txt, so this only downloads; we define the target.
    FetchContent_MakeAvailable(buddy)
    set(_buddy_src "${buddy_SOURCE_DIR}")
    message(STATUS "BuDDy: fetched into ${_buddy_src}")
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/buddy_config.h.in"
    "${CMAKE_BINARY_DIR}/buddy-generated/config.h"
    @ONLY)

# bddtest.cxx is upstream's own test program and carries a main().
add_library(buddy STATIC
    "${_buddy_src}/src/bddio.c"
    "${_buddy_src}/src/bddop.c"
    "${_buddy_src}/src/bvec.c"
    "${_buddy_src}/src/cache.c"
    "${_buddy_src}/src/cppext.cxx"
    "${_buddy_src}/src/fdd.c"
    "${_buddy_src}/src/imatrix.c"
    "${_buddy_src}/src/kernel.c"
    "${_buddy_src}/src/pairs.c"
    "${_buddy_src}/src/prime.c"
    "${_buddy_src}/src/reorder.c"
    "${_buddy_src}/src/tree.c")

target_include_directories(buddy
    PUBLIC  "${_buddy_src}/src"
    PRIVATE "${CMAKE_BINARY_DIR}/buddy-generated")

# Upstream C code is from 1996 and warns; that is not our code to fix.
target_compile_options(buddy PRIVATE -w)
set_target_properties(buddy PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(BuDDy::bdd ALIAS buddy)
