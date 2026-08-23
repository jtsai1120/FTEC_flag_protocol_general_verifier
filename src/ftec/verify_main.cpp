#include "ftec/dag.hpp"
#include "ftec/verify.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

void usage(const char* argv0) {
    std::cerr
        << "usage: " << argv0 << " <protocol.fpdl> [options]\n"
        << "  --backend=NAME   mock (default) -- see below\n"
        << "  --bound=N        fix the BMC bound instead of searching for one\n"
        << "  --max-paths=N    give up past this many symbolic paths (default 5000)\n"
        << "  --first          stop at the first unprotected path\n"
        << "  --dag            print the merged path structure and exit\n"
        << "\n"
        << "Backends:\n"
        << "  mock   runs the traversal without modelling errors: every branch is\n"
        << "         taken and nothing ever fails. Useful for inspecting a protocol's\n"
        << "         structure, not for deciding whether it is fault tolerant.\n"
        << "  dd     decision diagrams over Pauli sets. Not yet reachable from here:\n"
        << "         it needs the QASM front end to handle the dialect these\n"
        << "         protocols are written in (custom gates, reset, mid-circuit\n"
        << "         measure, three qubit registers).\n";
}

// The mock backend, as in the tests: every outcome, no failures. Kept here so
// the tool is useful before a physical backend is wired up.
class MockBackend : public ftec::Backend {
public:
    void begin(const fpdl::CodeSpec&, int) override { faults_.assign(1, 0); }
    StateId initial_state() override { return 0; }
    int fault_count(StateId id) const override { return faults_[id]; }

    std::vector<std::pair<ftec::Outcome, StateId>> step(
        StateId id, const ftec::CircuitRef& circuit) override {
        const bool flagged = circuit.flag_bits.has_value();
        std::vector<std::pair<ftec::Outcome, StateId>> out;
        for (int s = 0; s < 2; ++s) {
            for (int f = 0; f < (flagged ? 2 : 1); ++f) {
                ftec::Outcome outcome;
                outcome.syndrome = {s != 0};
                if (flagged) outcome.flag = {f != 0};
                faults_.push_back(faults_[id]);
                out.emplace_back(outcome, faults_.size() - 1);
            }
        }
        return out;
    }

    StateId merge(const std::vector<StateId>& states) override {
        faults_.push_back(faults_[states.front()]);
        return faults_.size() - 1;
    }

    std::optional<ftec::Failure> check(StateId) override { return std::nullopt; }

private:
    std::vector<int> faults_;
};

// Symbolic expansion needs a finite transition bound, and a protocol that has
// not finished expanding is silently incomplete. Rather than make the caller
// guess, raise the bound until the parser stops reporting truncation.
struct Expansion {
    fpdl::ParseResult result;
    std::size_t       bound = 0;
};

Expansion expand(const std::filesystem::path& source, std::optional<std::size_t> fixed,
                 std::size_t max_paths) {
    fpdl::ParseOptions options;
    options.max_paths = max_paths;

    if (fixed) {
        options.bmc_bound = *fixed;
        return {fpdl::Parser::parse_file(source, options), *fixed};
    }

    // Doubling keeps the number of attempts logarithmic; the cap is a safety
    // net for a protocol that genuinely never terminates.
    static constexpr std::size_t kCeiling = 1u << 16;
    for (std::size_t bound = 16; bound <= kCeiling; bound *= 2) {
        options.bmc_bound = bound;
        auto parsed = fpdl::Parser::parse_file(source, options);
        const bool bounded_out = parsed.truncated;
        bool any_path_bounded = false;
        for (const auto& path : parsed.paths) any_path_bounded |= path.bound_exceeded;
        if (!bounded_out && !any_path_bounded) return {std::move(parsed), bound};
    }
    throw std::runtime_error(
        "symbolic expansion did not converge below a bound of " + std::to_string(kCeiling) +
        "; pass --bound=N to choose one explicitly");
}

void print_dag(const ftec::Dag& dag) {
    std::map<ftec::DagNode::Kind, std::size_t> kinds;
    for (const auto& node : dag.nodes) ++kinds[node.kind];

    std::cout << "protocol   : " << dag.protocol << "\n"
              << "code       : [[" << dag.code.n << ',' << dag.code.k << ',' << dag.code.d
              << "]], tau = " << dag.code.fault_budget() << "\n"
              << "paths      : " << dag.path_count << "\n"
              << "nodes      : " << dag.nodes.size() << " ("
              << kinds[ftec::DagNode::Kind::Se] << " SE, "
              << kinds[ftec::DagNode::Kind::Terminal] << " terminal)\n"
              << "edges      : " << dag.edges.size() << "\n\n";

    for (const auto& node : dag.nodes) {
        if (node.kind != ftec::DagNode::Kind::Se) continue;
        std::cout << "  #" << node.id << "  round " << node.round << '.' << node.invocation
                  << ' ' << node.phase << "  " << node.circuit.se_name << "  ("
                  << node.circuit.qasm.filename().string() << ")\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    std::filesystem::path      source;
    std::string                backend_name = "mock";
    std::optional<std::size_t> bound;
    std::size_t                max_paths = 5000;
    bool                       dag_only  = false;
    ftec::VerifyOptions        options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--backend=", 0) == 0) {
            backend_name = arg.substr(10);
        } else if (arg.rfind("--bound=", 0) == 0) {
            bound = std::stoull(arg.substr(8));
        } else if (arg.rfind("--max-paths=", 0) == 0) {
            max_paths = std::stoull(arg.substr(12));
        } else if (arg == "--first") {
            options.stop_at_first_failure = true;
        } else if (arg == "--dag") {
            dag_only = true;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "error: unknown option " << arg << "\n";
            usage(argv[0]);
            return 2;
        } else if (source.empty()) {
            source = arg;
        } else {
            std::cerr << "error: more than one protocol given\n";
            return 2;
        }
    }
    if (source.empty()) {
        usage(argv[0]);
        return 2;
    }

    try {
        const auto expansion = expand(source, bound, max_paths);
        const auto dag = ftec::build_dag(expansion.result, source.parent_path());

        if (dag_only) {
            print_dag(dag);
            std::cout << "\nBMC bound  : " << expansion.bound
                      << (bound ? " (given)" : " (found by doubling)") << "\n";
            return 0;
        }

        if (backend_name == "dd") {
            std::cerr
                << "error: the dd backend is not reachable from this tool yet.\n"
                   "       Its QASM front end still expects a restricted dialect: two qubit\n"
                   "       registers named qd/qm, no custom gate definitions, no reset and\n"
                   "       no mid-circuit measure. These protocols use all four. Run\n"
                   "       build/backends/dd/dd-propagate directly for circuits in that\n"
                   "       dialect, or use --backend=mock to inspect the path structure.\n";
            return 1;
        }
        if (backend_name != "mock") {
            std::cerr << "error: unknown backend '" << backend_name << "'\n";
            usage(argv[0]);
            return 2;
        }

        MockBackend backend;
        const auto  result = ftec::verify(dag, backend, options);

        std::cout << "protocol        : " << result.protocol << "\n"
                  << "code            : [[" << dag.code.n << ',' << dag.code.k << ','
                  << dag.code.d << "]], tau = " << result.tau << "\n"
                  << "BMC bound       : " << expansion.bound
                  << (bound ? " (given)" : " (found by doubling)") << "\n"
                  << "backend         : " << backend_name << "\n"
                  << "paths reached   : " << result.paths_reached << " of " << dag.path_count
                  << "\n"
                  << "circuits run    : " << result.se_applications << "\n\n";

        if (result.clean()) {
            std::cout << "no unprotected path found";
            if (backend_name == "mock") {
                std::cout << " -- but the mock backend never reports one, so this says\n"
                             "nothing about fault tolerance; it only means the traversal "
                             "completed";
            }
            std::cout << "\n";
            return 0;
        }

        std::cout << result.failures.size() << " unprotected path(s); first failure at t = "
                  << result.min_fault_count << "\n\n";
        for (const auto& failure : result.failures) {
            std::cout << "  path " << failure.path_id << "  t=" << failure.failure.fault_count
                      << "  record=" << failure.record_string() << "\n";
            if (!failure.failure.detail.empty()) {
                std::cout << "        " << failure.failure.detail << "\n";
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
