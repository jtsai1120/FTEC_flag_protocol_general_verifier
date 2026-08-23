#include "flow_check.hpp"

#include <stdexcept>
#include <string>

namespace pbdd {

namespace {

void require_matching_width(const StabilizerCode &code, int n_data, const char *who) {
    if (code.n_data() != n_data) {
        throw std::invalid_argument(std::string(who) + ": the code covers "
                                    + std::to_string(code.n_data())
                                    + " data qubits but the circuit declares "
                                    + std::to_string(n_data));
    }
}

} // namespace

FlowCheckResult check_branches(const StabilizerCode &code,
                               const std::vector<SyndromeBranch> &branches,
                               bool stop_at_first_failure) {
    FlowCheckResult result;

    for (const SyndromeBranch &b : branches) {
        ++result.branches_checked;

        LogicalCollision hit = find_undetectable_logical_pair(code, b.set);
        if (!hit.found) continue;

        if (result.min_fault_count < 0 || b.t < result.min_fault_count) {
            result.min_fault_count = b.t;
        }
        result.failures.push_back(FlowFailure{b.t, b.mr, std::move(hit)});

        if (stop_at_first_failure) break;
    }
    return result;
}

FlowCheckResult check_flow(const StabilizerCode &code, const PauliFlow &flow,
                           bool stop_at_first_failure) {
    require_matching_width(code, flow.n_data(), "check_flow");
    return check_branches(code, flow.branches(), stop_at_first_failure);
}

FlowCheckResult check_flow(const StabilizerCode &code, const QasmPropagation &run,
                           bool stop_at_first_failure) {
    require_matching_width(code, run.n_data(), "check_flow");
    return check_branches(code, run.branches(), stop_at_first_failure);
}

} // namespace pbdd
