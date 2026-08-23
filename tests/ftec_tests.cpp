#include "ftec/dag.hpp"
#include "ftec/verify.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (condition) return;
    std::cerr << "  FAILED: " << what << '\n';
    ++failures;
}

// ---------------------------------------------------------------------------
// A backend that models nothing physical, only the *shape* of one.
//
// A real backend returns the outcomes a circuit can actually produce, and that
// set is small because the fault budget is small: with tau faults only so many
// measurements can deviate from what a fault-free run would report. This one
// mimics exactly that constraint and nothing else -- it starts from the
// all-zero outcome and will report a deviating one only while fewer than tau
// deviations have happened along the path.
//
// The budget is not decoration. Without it the mock claims every register
// value at every step, which is 4^depth records for a protocol like CB18 and
// says nothing about whether the driver is right; with it, the walk is bounded
// the way a real one is, and both sides of every guard still get exercised.
// ---------------------------------------------------------------------------
class MockBackend : public ftec::Backend {
public:
    explicit MockBackend(int budget = -1) : budget_(budget) {}

    void begin(const fpdl::CodeSpec&, int tau) override {
        tau_ = budget_ < 0 ? tau : budget_;
        used_.assign(1, 0);
        steps_.clear();
    }

    StateId initial_state() override { return 0; }

    std::vector<std::pair<ftec::Outcome, StateId>> step(StateId id,
                                                        const ftec::CircuitRef& circuit) override {
        steps_.push_back(circuit.se_name);
        const bool flagged = circuit.flag_bits.has_value();

        std::vector<std::pair<ftec::Outcome, StateId>> out;
        const auto emit = [&](bool s, bool f, int cost) {
            if (used_[id] + cost > tau_) return;
            ftec::Outcome outcome;
            outcome.syndrome = {s};
            if (flagged) outcome.flag = {f};
            used_.push_back(used_[id] + cost);
            out.emplace_back(std::move(outcome), used_.size() - 1);
        };
        emit(false, false, 0);
        emit(true, false, 1);
        if (flagged) {
            emit(false, true, 1);
            emit(true, true, 1);
        }
        return out;
    }

    std::optional<ftec::Failure> check(StateId) override { return std::nullopt; }

    const std::vector<std::string>& steps() const { return steps_; }

private:
    int                      budget_ = -1;
    int                      tau_ = 0;
    std::vector<int>         used_;
    std::vector<std::string> steps_;
};

// A backend that fails on demand, to check the reporting and the early exit.
class FailingBackend : public MockBackend {
public:
    explicit FailingBackend(std::size_t fail_after) : fail_after_(fail_after) {}

    std::optional<ftec::Failure> check(StateId) override {
        if (terminals_++ < fail_after_) return std::nullopt;
        return ftec::Failure{1, "synthetic"};
    }

private:
    std::size_t fail_after_ = 0;
    std::size_t terminals_  = 0;
};

ftec::Dag load(const std::filesystem::path& fpdl, std::size_t bound = 200) {
    fpdl::ParseOptions options;
    options.bmc_bound = bound;
    options.max_paths = 5000;
    const auto parsed = fpdl::Parser::parse_file(fpdl, options);
    return ftec::build_dag(parsed, fpdl.parent_path());
}

std::size_t count_kind(const ftec::Dag& dag, ftec::DagNode::Kind kind) {
    std::size_t n = 0;
    for (const auto& node : dag.nodes) {
        if (node.kind == kind) ++n;
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    // How many deviating measurements the mock will invent along a path.
    //
    // Two is the smallest that reaches every symbolic path of every protocol
    // here, which is what the coverage checks below need. It is a search bound
    // and not a fault model -- one physical fault can make several rounds
    // report a non-zero syndrome, since the error it leaves behind persists --
    // so it is chosen by measurement rather than derived: at one, four of
    // Bha23's thirteen paths and most of CB18's are unreachable; at three,
    // CB18 goes from 4852 records to 88444 and covers nothing new.
    int kMockBudget = 2;
    if (argc > 2 && std::string(argv[1]) == "--budget") {
        kMockBudget = std::stoi(argv[2]);
        argv += 2;
        argc -= 2;
    }
    if (argc < 2) {
        std::cerr << "usage: ftec_tests <protocol.fpdl>...\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        const std::filesystem::path source = argv[i];
        std::cout << source.filename().string() << '\n';

        const ftec::Dag dag = load(source);

        // --- the merged paths really are a trie -----------------------------
        // Every node bar the root has exactly one parent. This is what lets a
        // depth-first walk visit each SE once, and it is worth asserting
        // rather than assuming: a builder bug that re-attached a node would
        // quietly turn the walk into path enumeration.
        std::map<std::size_t, std::size_t> in_degree;
        for (const auto& edge : dag.edges) ++in_degree[edge.to];
        check(dag.edges.size() == dag.nodes.size() - 1,
              "edges == nodes - 1 (a tree)");
        for (const auto& [node, degree] : in_degree) {
            check(degree == 1, "node " + std::to_string(node) + " has one parent");
        }
        check(in_degree.count(dag.root()) == 0, "the root has no parent");

        const auto terminals = count_kind(dag, ftec::DagNode::Kind::Terminal);
        const auto se_nodes  = count_kind(dag, ftec::DagNode::Kind::Se);
        check(terminals == dag.path_count, "one terminal per path");

        // Every SE node must carry enough to actually run something.
        for (const auto& node : dag.nodes) {
            if (node.kind != ftec::DagNode::Kind::Se) continue;
            check(!node.circuit.se_name.empty(), "SE node names its circuit");
            check(std::filesystem::exists(node.circuit.qasm),
                  "QASM resolves: " + node.circuit.qasm.string());
            check(!node.circuit.data_qubits.empty(), "SE node names qp");
            check(!node.circuit.syndrome_qubits.empty(), "SE node names qm");
            check(node.circuit.flag_qubits.has_value() == node.circuit.flag_bits.has_value(),
                  "qf and cf agree");
        }

        // --- the walk reaches every path, running each SE once --------------
        MockBackend mock(kMockBudget);
        const auto  result = ftec::verify(dag, mock);

        check(result.paths_reached == dag.path_count,
              "reached every symbolic path (" + std::to_string(result.paths_reached) + " of " +
                  std::to_string(dag.path_count) + ")");
        check(result.records_reached >= result.paths_reached,
              "records are at least as many as the paths they realise");
        // A node is stepped once per distinct record that reaches it, not once
        // outright: distinct records carry distinct states and cannot share a
        // circuit run. What the trie saves is re-walking a shared *prefix* for
        // every path that continues through it, so the count sits between the
        // number of SE nodes and what enumerating paths would cost.
        check(result.se_applications >= se_nodes,
              "every SE node was stepped at least once");
        check(result.clean(), "a backend that never fails reports no failures");
        check(result.tau == dag.code.fault_budget(), "tau comes from the distance");

        // Enumerating paths instead would cost the sum of their lengths. That
        // number is what the trie buys; report it so a regression is visible.
        std::size_t enumerated = 0;
        {
            std::vector<std::pair<std::size_t, std::size_t>> stack{{dag.root(), 0}};
            while (!stack.empty()) {
                const auto [node, depth] = stack.back();
                stack.pop_back();
                const auto& current = dag.nodes[node];
                if (current.kind == ftec::DagNode::Kind::Terminal) {
                    enumerated += depth;
                    continue;
                }
                const std::size_t next =
                    depth + (current.kind == ftec::DagNode::Kind::Se ? 1 : 0);
                for (const auto child : dag.successors(node)) stack.emplace_back(child, next);
            }
        }
        std::cout << "  paths " << dag.path_count << " (" << result.records_reached
                  << " records), SE nodes " << se_nodes << ", circuits run "
                  << result.se_applications << "; enumerating the merged paths alone would "
                  << "revisit " << enumerated << " SE nodes\n";
        check(enumerated >= se_nodes, "sharing never costs more than enumeration");
    }

    // --- failure reporting and early exit -----------------------------------
    {
        const std::filesystem::path source = argv[1];
        const ftec::Dag             dag    = load(source);

        FailingBackend always_fails(0);
        const auto     all = ftec::verify(dag, always_fails);
        check(!all.clean(), "failures are reported");
        check(all.failures.size() == dag.path_count, "every path reported once");
        check(all.min_fault_count == 1, "min_fault_count comes from the failures");
        for (const auto& failure : all.failures) {
            check(!failure.record.empty(), "a failure carries the record that reached it");
        }

        FailingBackend stops(0);
        ftec::VerifyOptions options;
        options.stop_at_first_failure = true;
        const auto early = ftec::verify(dag, stops, options);
        check(early.failures.size() == 1, "early exit stops after one failure");
        check(early.min_fault_count == all.min_fault_count,
              "early exit does not change min_fault_count");
        check(early.paths_reached < all.paths_reached || dag.path_count == 1,
              "early exit really did less work");
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\nAll ftec tests passed.\n";
    return 0;
}
