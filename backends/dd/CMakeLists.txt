# The decision-diagram backend: Pauli sets as BDDs over BuDDy.
#
# This directory arrived as its own repository (see the subtree merge in the
# history) and still builds standalone; document/ describes the design. What
# the rest of the project consumes is the ftec_dd library plus the ftec::Backend
# implementation layered on top of it.

add_library(ftec_dd
    src/pauli_bdd.cpp
    src/qasm_propagate.cpp
    src/stabilizer.cpp
    src/flow_check.cpp
    src/dd_backend.cpp)

target_include_directories(ftec_dd PUBLIC include)
target_link_libraries(ftec_dd PUBLIC BuDDy::bdd ftec_core)
target_compile_options(ftec_dd PRIVATE -Wall -Wextra)

# The standalone driver this directory came with. Superseded by the top-level
# CLI once the DAG traversal lands, but kept: it is the quickest way to run a
# single circuit chain without going through FPDL.
add_executable(dd-propagate src/propagate_main.cpp)
target_link_libraries(dd-propagate PRIVATE ftec_dd)
target_compile_options(dd-propagate PRIVATE -Wall -Wextra)

if(BUILD_TESTING)
    # Each of these is self-checking: it aborts on the first failed assertion
    # and most compare against an independent implementation of the same thing.
    foreach(_t demo reorder_test gates_test subgroup_test qasm_test
               stabilizer_test flow_check_test)
        add_executable(dd_${_t} test/${_t}.cpp)
        target_link_libraries(dd_${_t} PRIVATE ftec_dd)
        # -UNDEBUG is not optional here. These tests check everything through
        # bare assert(), which a Release build defines away -- and a suite that
        # reports success while checking nothing is worse than no suite at all.
        target_compile_options(dd_${_t} PRIVATE -Wall -Wextra -UNDEBUG)
        # qasm_test and flow_check_test write scratch .qasm files beside the
        # working directory, so give them one of their own.
        add_test(NAME dd.${_t}
                 COMMAND dd_${_t}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endforeach()
endif()
