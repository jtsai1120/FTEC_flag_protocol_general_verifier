#pragma once

#include "qasm_propagate.hpp"
#include "stabilizer.hpp"

#include <string>
#include <vector>

namespace pbdd {

// One path that the code cannot protect: at fault count t, having observed the
// record mr, the branch holds two errors whose product is a non-trivial logical
// operator, so no recovery can handle both.
struct FlowFailure {
    int              t = 0;
    std::string      mr;
    LogicalCollision collision;
};

struct FlowCheckResult {
    int                      branches_checked = 0;
    int                      min_fault_count  = -1;  // smallest failing t, -1 if none
    std::vector<FlowFailure> failures;               // ordered as the branches were

    bool clean() const { return failures.empty(); }
};

// Run the N(S)\S query over every branch of a propagation.
//
// min_fault_count is the headline number: the fewest faults that can make the
// circuit fail. If it comes back -1 the circuit tolerates every fault pattern
// the propagation explored, i.e. up to tau faults.
//
// t = 0 can never fail: that branch holds the single fault-free Pauli frame,
// and one element has no partner to multiply with.
//
// stop_at_first_failure exits as soon as a failing branch is found. Branches
// arrive sorted by (t, mr), so the first failure is already at the smallest t
// and min_fault_count is still correct -- only the list of other failing paths
// is cut short.
FlowCheckResult check_branches(const StabilizerCode &code,
                               const std::vector<SyndromeBranch> &branches,
                               bool stop_at_first_failure = false);

// Same, with a check that the code and the flow agree on the data register.
FlowCheckResult check_flow(const StabilizerCode &code, const PauliFlow &flow,
                           bool stop_at_first_failure = false);

FlowCheckResult check_flow(const StabilizerCode &code, const QasmPropagation &run,
                           bool stop_at_first_failure = false);

} // namespace pbdd
