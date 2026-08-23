# ==============================================================================
# PauliSetBDD Makefile
#
# Assumes BuDDy has already been built (see document/README.md for how to fetch
# and build it). Point BUDDY_DIR at the *source* tree root (the directory that
# contains src/bdd.h and src/.libs/libbdd.a after building), or override
# BUDDY_INC / BUDDY_LIB directly if you installed BuDDy system-wide.
#
# Every binary is written into test/, next to the sources it is built from.
# ==============================================================================

BUDDY_DIR := buddy_src
BUDDY_INC := $(BUDDY_DIR)/src
BUDDY_LIB := $(BUDDY_DIR)/src/.libs/libbdd.a

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I include -I $(BUDDY_INC)

SRC      := src/pauli_bdd.cpp
QASM_SRC := src/qasm_propagate.cpp
STAB_SRC := src/stabilizer.cpp
FLOW_SRC := src/flow_check.cpp
HEADER   := include/pauli_bdd.hpp
HEADERS  := $(HEADER) include/qasm_propagate.hpp
STAB_HDR := $(HEADER) include/stabilizer.hpp
FLOW_HDR := $(HEADERS) include/stabilizer.hpp include/flow_check.hpp
FULL_SRC := $(SRC) $(QASM_SRC) $(STAB_SRC) $(FLOW_SRC)

BIN_DIR := test
TESTS   := $(BIN_DIR)/demo $(BIN_DIR)/reorder_test $(BIN_DIR)/gates_test \
           $(BIN_DIR)/subgroup_test $(BIN_DIR)/qasm_test \
           $(BIN_DIR)/stabilizer_test $(BIN_DIR)/flow_check_test
TOOLS   := $(BIN_DIR)/propagate

.PHONY: all check clean

all: $(TESTS) $(TOOLS)

# Run every test and stop at the first failure.
check: $(TESTS)
	@for t in $(TESTS); do \
	    printf '%-24s' "$$t"; \
	    if ./$$t > /dev/null 2>&1; then echo "ok"; else echo "FAILED"; exit 1; fi; \
	done

$(BIN_DIR)/demo: $(SRC) test/demo.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) $(SRC) test/demo.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/reorder_test: $(SRC) test/reorder_test.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) $(SRC) test/reorder_test.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/gates_test: $(SRC) test/gates_test.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) $(SRC) test/gates_test.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/subgroup_test: $(SRC) test/subgroup_test.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) $(SRC) test/subgroup_test.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/qasm_test: $(SRC) $(QASM_SRC) test/qasm_test.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRC) $(QASM_SRC) test/qasm_test.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/stabilizer_test: $(SRC) $(STAB_SRC) test/stabilizer_test.cpp $(STAB_HDR)
	$(CXX) $(CXXFLAGS) $(SRC) $(STAB_SRC) test/stabilizer_test.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/flow_check_test: $(FULL_SRC) test/flow_check_test.cpp $(FLOW_HDR)
	$(CXX) $(CXXFLAGS) $(FULL_SRC) test/flow_check_test.cpp $(BUDDY_LIB) -o $@

$(BIN_DIR)/propagate: $(FULL_SRC) src/propagate_main.cpp $(FLOW_HDR)
	$(CXX) $(CXXFLAGS) $(FULL_SRC) src/propagate_main.cpp $(BUDDY_LIB) -o $@

clean:
	rm -f $(TESTS) $(TOOLS)
