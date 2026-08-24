#include "flow_check.hpp"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void usage(const char *argv0) {
    std::cerr << "usage: " << argv0
              << " <tau> <circuit.qasm> [more.qasm ...] [--code=FILE] [--list[=N]]\n"
              << "  tau         total fault budget across the whole chain\n"
              << "  --code=F    stabilizer generators, one Pauli string per line\n"
              << "              (blank lines and #/// comments ignored); each branch is\n"
              << "              then checked for a pair whose product lies in N(S)\\S\n"
              << "  --list[=N]  print the data-qubit Pauli strings in each branch, at\n"
              << "              most N per branch (default 32)\n";
}

std::vector<std::string> read_generators(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open code file: " + path);

    std::vector<std::string> gens;
    std::string              line;
    while (std::getline(in, line)) {
        const size_t hash = line.find_first_of('#');
        if (hash != std::string::npos) line.erase(hash);
        const size_t slashes = line.find("//");
        if (slashes != std::string::npos) line.erase(slashes);

        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        gens.push_back(line.substr(b, e - b + 1));
    }
    if (gens.empty()) throw std::runtime_error("code file has no generators: " + path);
    return gens;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    const int tau = std::atoi(argv[1]);

    std::vector<std::string> circuits;
    std::string              code_path;
    bool                     list  = false;
    std::size_t              limit = 32;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list") {
            list = true;
        } else if (arg.rfind("--list=", 0) == 0) {
            list  = true;
            limit = static_cast<std::size_t>(std::atoi(arg.c_str() + 7));
        } else if (arg.rfind("--code=", 0) == 0) {
            code_path = arg.substr(7);
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "error: unknown option " << arg << "\n";
            usage(argv[0]);
            return 2;
        } else {
            circuits.push_back(arg);
        }
    }

    if (circuits.empty()) {
        usage(argv[0]);
        return 2;
    }

    try {
        pbdd::PauliFlow flow(tau);
        for (const std::string &path : circuits) flow.run(path);

        const int nd = flow.n_data();

        std::cout << "rounds     : " << flow.n_rounds() << "\n"
                  << "data qubits: " << nd << "\n"
                  << "ancillas   : " << flow.n_measure() << " (widest round)\n"
                  << "tau        : " << flow.tau() << " (whole chain)\n\n"
                  << flow.branches().size() << " surviving path(s)\n"
                  << "  t   mr              |set|      BDD nodes\n"
                  << "  ---------------------------------------------\n";

        for (const pbdd::SyndromeBranch &b : flow.branches()) {
            std::cout << "  " << b.t << "   " << b.mr << "\t\t" << b.set.size() << "\t\t"
                      << b.set.node_count() << "\n";
            if (!list) continue;

            // Only the data register is worth listing: the ancillas have been
            // reset to identity and whatever they carried is already in mr.
            // Distinct strings can share a data part, so de-duplicate.
            std::set<std::string> data;
            b.set.for_each([&](const std::string &s) {
                data.insert(s.substr(0, static_cast<std::size_t>(nd)));
                return data.size() <= limit;   // one extra tells us it overflowed
            });

            std::size_t shown = 0;
            for (const std::string &d : data) {
                if (shown++ == limit) break;
                std::cout << "        " << d << "\n";
            }
            if (data.size() > limit) std::cout << "        ...\n";
        }

        if (code_path.empty()) return 0;

        const pbdd::StabilizerCode code(nd, read_generators(code_path));
        std::cout << "\nstabilizer code: [[" << code.n_data() << "," << code.m()
                  << "]], " << code.k() << " generators from " << code_path << "\n";

        const pbdd::FlowCheckResult check = pbdd::check_flow(code, flow);

        if (check.clean()) {
            std::cout << "all " << check.branches_checked
                      << " path(s) protected up to tau = " << flow.tau() << "\n";
            return 0;
        }

        std::cout << check.failures.size() << " of " << check.branches_checked
                  << " path(s) unprotected; first failure at t = " << check.min_fault_count
                  << "\n\n";
        for (const pbdd::FlowFailure &f : check.failures) {
            const pbdd::LogicalCollision &c = f.collision;
            std::cout << "  FAIL  t=" << f.t << "  mr=" << f.mr << "  syndrome=" << c.syndrome
                      << "  logical " << c.logical_a << " vs " << c.logical_b << "\n"
                      << "        E1 = " << c.witness_1 << "\n"
                      << "        E2 = " << c.witness_2 << "\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
