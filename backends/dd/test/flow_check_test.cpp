#include "flow_check.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace pbdd;

namespace {

const char *kTmp = "._flow_check_tmp.qasm";

void write_qasm(const std::string &body) {
    std::ofstream out(kTmp);
    out << body;
}

// ---------------------------------------------------------------------------
// Brute force over the actual branch contents: enumerate every element, drop
// the ancillas, and multiply every pair. Shares nothing with the BDD path.
// ---------------------------------------------------------------------------

using Bits = std::vector<int>;

Bits to_bits(const std::string &p) {
    Bits v(p.size() * 2, 0);
    for (size_t q = 0; q < p.size(); ++q) {
        const int c = static_cast<int>(PauliSetBDD::char_to_pauli(p[q]));
        v[2 * q]     = c & 1;
        v[2 * q + 1] = (c >> 1) & 1;
    }
    return v;
}

int symp(const Bits &u, const Bits &v) {
    int r = 0;
    for (size_t i = 0; i + 1 < u.size(); i += 2) r ^= (u[i] & v[i + 1]) ^ (u[i + 1] & v[i]);
    return r & 1;
}

Bits xor_bits(const Bits &a, const Bits &b) {
    Bits o(a.size());
    for (size_t i = 0; i < a.size(); ++i) o[i] = a[i] ^ b[i];
    return o;
}

bool in_span(const std::vector<Bits> &gens, Bits v) {
    std::vector<Bits> basis;
    for (const Bits &g : gens) {
        Bits cur = g;
        for (const Bits &b : basis) {
            size_t lead = 0;
            while (lead < b.size() && !b[lead]) ++lead;
            if (lead < cur.size() && cur[lead]) cur = xor_bits(cur, b);
        }
        bool nonzero = false;
        for (int x : cur) nonzero |= x != 0;
        if (nonzero) basis.push_back(cur);
    }
    for (const Bits &b : basis) {
        size_t lead = 0;
        while (lead < b.size() && !b[lead]) ++lead;
        if (lead < v.size() && v[lead]) v = xor_bits(v, b);
    }
    for (int x : v) {
        if (x) return false;
    }
    return true;
}

bool brute_force_branch(const std::vector<std::string> &gens, const PauliSetBDD &set, int nd) {
    // Elements, truncated to the data register and de-duplicated.
    std::set<std::string> data;
    set.for_each([&](const std::string &s) {
        data.insert(s.substr(0, static_cast<size_t>(nd)));
        return true;
    });
    const std::vector<std::string> elements(data.begin(), data.end());

    std::vector<Bits> gb;
    for (const std::string &g : gens) gb.push_back(to_bits(g));

    for (size_t i = 0; i < elements.size(); ++i) {
        for (size_t j = i + 1; j < elements.size(); ++j) {
            const Bits prod = xor_bits(to_bits(elements[i]), to_bits(elements[j]));
            bool       in_n = true;
            for (const Bits &g : gb) in_n &= symp(g, prod) == 0;
            if (in_n && !in_span(gb, prod)) return true;
        }
    }
    return false;
}

void check_end_to_end(const std::string &qasm, const std::vector<std::string> &gens, int tau,
                      const char *label) {
    write_qasm(qasm);
    PauliFlow flow(tau);
    flow.run(kTmp);

    const StabilizerCode  code(flow.n_data(), gens);
    const FlowCheckResult got = check_flow(code, flow);

    int expected_min = -1;
    int expected_n   = 0;
    for (const SyndromeBranch &b : flow.branches()) {
        if (!brute_force_branch(gens, b.set, flow.n_data())) continue;
        ++expected_n;
        if (expected_min < 0 || b.t < expected_min) expected_min = b.t;
    }

    assert(got.branches_checked == static_cast<int>(flow.branches().size()));
    assert(got.min_fault_count == expected_min);
    assert(static_cast<int>(got.failures.size()) == expected_n);

    // Every reported witness pair really is a failure, and really is present.
    for (const FlowFailure &f : got.failures) {
        assert(f.collision.found);
        assert(f.collision.logical_a != f.collision.logical_b);
        bool matched = false;
        for (const SyndromeBranch &b : flow.branches()) {
            if (b.t == f.t && b.mr == f.mr) {
                matched = true;
                assert(b.set.contains(f.collision.witness_1
                                      + std::string(static_cast<size_t>(flow.n_measure()), 'I')));
                assert(b.set.contains(f.collision.witness_2
                                      + std::string(static_cast<size_t>(flow.n_measure()), 'I')));
            }
        }
        assert(matched);
    }

    // Early exit must not change the headline number.
    const FlowCheckResult early = check_flow(code, flow, /*stop_at_first_failure=*/true);
    assert(early.min_fault_count == expected_min);
    assert(early.failures.size() <= got.failures.size());

    std::cout << "    " << label << ": " << got.failures.size() << "/"
              << got.branches_checked << " paths fail, min t = " << got.min_fault_count
              << " (matches brute force)\n";
}

const std::vector<std::string> kFive = {"XZZXI", "IXZZX", "XIXZZ", "ZXIXZ"};

} // namespace

int main() {
    std::cout << "== end-to-end: propagate, split, then check every path ==\n";

    // A circuit that never entangles anything: only the fault-free frame
    // survives at t = 0, so nothing can fail.
    check_end_to_end("OPENQASM 3.0;\nqubit[5] qd;\nqubit[1] qm;\nh qd[0];\nz qd[1];\n",
                     kFive, 2, "no two-qubit gates, tau=2");

    // One fault location: t = 1 branches hold whole cosets, which is plenty to
    // contain an undetectable pair.
    check_end_to_end("OPENQASM 3.0;\nqubit[5] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\n",
                     kFive, 1, "single cx, tau=1");

    check_end_to_end("OPENQASM 3.0;\nqubit[5] qd;\nqubit[1] qm;\n"
                     "cx qd[0], qm[0];\ncx qd[1], qm[0];\nh qd[2];\n",
                     kFive, 1, "two fault locations, tau=1");

    check_end_to_end("OPENQASM 3.0;\nqubit[5] qd;\nqubit[2] qm;\n"
                     "cx qd[0], qm[0];\ncy qd[1], qm[1];\ncz qd[2], qd[3];\n",
                     kFive, 1, "cx/cy/cz mixed, tau=1");

    std::cout << "\n== structural checks ==\n";
    {
        write_qasm("OPENQASM 3.0;\nqubit[5] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\n");
        PauliFlow flow(1);
        flow.run(kTmp);
        const StabilizerCode  code(5, kFive);
        const FlowCheckResult r = check_flow(code, flow);

        // t = 0 holds a single Pauli, so it can never be among the failures.
        for (const FlowFailure &f : r.failures) assert(f.t > 0);
        std::cout << "    t = 0 never fails (one element has no partner)\n";

        // Failures are reported in branch order, so min_fault_count is the t of
        // the first one.
        if (!r.clean()) assert(r.failures.front().t == r.min_fault_count);
        std::cout << "    failures come back in (t, mr) order\n";
    }
    {
        // The code must match the circuit's data register.
        write_qasm("OPENQASM 3.0;\nqubit[4] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\n");
        PauliFlow flow(1);
        flow.run(kTmp);
        const StabilizerCode code(5, kFive);
        bool                 threw = false;
        try { check_flow(code, flow); } catch (const std::invalid_argument &e) {
            threw = true;
            std::cout << "    rejected a width mismatch: " << e.what() << "\n";
        }
        assert(threw);
    }
    {
        // A tau=0 chain keeps exactly one path and it cannot fail, however many
        // rounds run.
        write_qasm("OPENQASM 3.0;\nqubit[5] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\n");
        PauliFlow flow(0);
        flow.run(kTmp);
        flow.run(kTmp);
        const StabilizerCode  code(5, kFive);
        const FlowCheckResult r = check_flow(code, flow);
        assert(r.branches_checked == 1 && r.clean() && r.min_fault_count == -1);
        std::cout << "    tau = 0 over two rounds: one path, clean\n";
    }
    {
        // QasmPropagation takes the same query.
        write_qasm("OPENQASM 3.0;\nqubit[5] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\n");
        QasmPropagation       run  = propagate_qasm(kTmp, 1);
        const StabilizerCode  code(5, kFive);
        const FlowCheckResult r = check_flow(code, run);
        assert(r.branches_checked == static_cast<int>(run.branches().size()));
        std::cout << "    works on a single QasmPropagation too\n";
    }

    std::remove(kTmp);
    std::cout << "\nAll flow-check tests passed.\n";
    return 0;
}
