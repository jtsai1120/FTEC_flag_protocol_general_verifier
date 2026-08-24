#include "qasm_propagate.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace pbdd;

namespace {

const char *kTmp = "._qasm_test_tmp.qasm";

void write_qasm(const std::string &body) {
    std::ofstream out(kTmp);
    out << body;
}

bool rejects(const std::string &body, const char *what) {
    write_qasm(body);
    try {
        QasmPropagation p = propagate_qasm(kTmp, 1);
    } catch (const std::exception &e) {
        std::cout << "    rejected " << what << ": " << e.what() << "\n";
        return true;
    }
    std::cout << "    !! ACCEPTED " << what << " but should not have\n";
    return false;
}

// ---------------------------------------------------------------------------
// Independent reference: same algorithm, explicit sets of Pauli strings instead
// of BDDs. Validates the tic / spawn / merge bookkeeping and the syndrome split
// against the BDD path. Encoding matches pbdd::Pauli -- bit0 = X, bit1 = Z.
// ---------------------------------------------------------------------------

int to_bits(char c) {
    switch (c) {
        case 'I': return 0;
        case 'X': return 1;
        case 'Z': return 2;
        case 'Y': return 3;
    }
    throw std::runtime_error("bad Pauli char");
}
char to_char(int v) { return "IXZY"[v]; }

int  xbit(const std::string &s, int q) { return to_bits(s[static_cast<size_t>(q)]) & 1; }
int  zbit(const std::string &s, int q) { return (to_bits(s[static_cast<size_t>(q)]) >> 1) & 1; }
void set_bits(std::string &s, int q, int x, int z) {
    s[static_cast<size_t>(q)] = to_char((z << 1) | x);
}

std::string ref_gate(std::string p, const std::string &kind, int a, int b) {
    // Every gate conjugates. For x and z that is the identity on a phase-free
    // Pauli, so they are listed and do nothing -- circuits below still contain
    // them, which is exactly what makes both implementations agree they are
    // inert rather than agree by never meeting one.
    if (kind == "x" || kind == "z") { (void)a; }
    else if (kind == "h") set_bits(p, a, zbit(p, a), xbit(p, a));
    else if (kind == "cx") {
        const int xc = xbit(p, a), zt = zbit(p, b);
        set_bits(p, b, xbit(p, b) ^ xc, zbit(p, b));
        set_bits(p, a, xbit(p, a), zbit(p, a) ^ zt);
    } else if (kind == "cy") {
        // z_c ^= x_t ^ z_t, x_t ^= x_c, z_t ^= x_c -- all from the old values,
        // which is exactly why the BDD side needs a simultaneous substitution.
        const int xc = xbit(p, a), xt = xbit(p, b), zt = zbit(p, b);
        set_bits(p, a, xbit(p, a), zbit(p, a) ^ xt ^ zt);
        set_bits(p, b, xt ^ xc, zt ^ xc);
    } else if (kind == "cz") {
        const int xc = xbit(p, a), xt = xbit(p, b);
        set_bits(p, a, xbit(p, a), zbit(p, a) ^ xt);
        set_bits(p, b, xbit(p, b), zbit(p, b) ^ xc);
    } else throw std::runtime_error("bad gate");
    return p;
}

std::set<std::string> ref_all_faults(const std::string &p, int a, int b) {
    std::set<std::string> out;
    for (int ga = 0; ga < 4; ++ga) {
        for (int gb = 0; gb < 4; ++gb) {
            std::string q = p;
            set_bits(q, a, xbit(p, a) ^ (ga & 1), zbit(p, a) ^ ((ga >> 1) & 1));
            set_bits(q, b, xbit(p, b) ^ (gb & 1), zbit(p, b) ^ ((gb >> 1) & 1));
            out.insert(q);
        }
    }
    return out;
}

// Z-basis measurement: qm[j] flips the outcome exactly when its Pauli
// anticommutes with Z, i.e. when the x component is set (X and Y, not I or Z).
std::string ref_syndrome(const std::string &p, int nd, int nm) {
    std::string s(static_cast<size_t>(nm), '0');
    for (int j = 0; j < nm; ++j) s[static_cast<size_t>(j)] = xbit(p, nd + j) ? '1' : '0';
    return s;
}

// A reset erases the ancillas: their x part is already recorded in mr and
// their z part cannot influence anything downstream.
std::string ref_reset(std::string p, int nd, int nm) {
    for (int j = 0; j < nm; ++j) p[static_cast<size_t>(nd + j)] = 'I';
    return p;
}

struct RefGate {
    std::string kind;
    int a, b;
};

void reference_tics(std::vector<std::set<std::string>> &by_t,
                    const std::vector<RefGate> &gates, int tau) {
    for (const RefGate &g : gates) {
        for (auto &level : by_t) {                                            // (A)
            std::set<std::string> next;
            for (const auto &p : level) next.insert(ref_gate(p, g.kind, g.a, g.b));
            level = next;
        }
        if (g.kind != "cx" && g.kind != "cy" && g.kind != "cz") continue;

        std::vector<std::set<std::string>> spawn(static_cast<size_t>(tau));    // (B)
        for (int t = 0; t < tau; ++t) {
            for (const auto &p : by_t[static_cast<size_t>(t)]) {
                const auto s = ref_all_faults(p, g.a, g.b);
                spawn[static_cast<size_t>(t)].insert(s.begin(), s.end());
            }
        }
        for (int t = 0; t < tau; ++t) {                                        // (C)
            by_t[static_cast<size_t>(t) + 1].insert(spawn[static_cast<size_t>(t)].begin(),
                                                    spawn[static_cast<size_t>(t)].end());
        }
    }
}

std::vector<std::set<std::string>> reference_propagate(
    int n, const std::vector<RefGate> &gates, int tau) {
    std::vector<std::set<std::string>> by_t(static_cast<size_t>(tau) + 1);
    by_t[0].insert(std::string(static_cast<size_t>(n), 'I'));
    reference_tics(by_t, gates, tau);
    return by_t;
}

// Reference for a whole chain: group by record, propagate each group as one
// t-indexed state, split, append to the record, reset, merge on (t, mr).
using RefState = std::map<std::pair<int, std::string>, std::set<std::string>>;

struct RefCircuit {
    int                  nm;
    std::vector<RefGate> gates;   // global indices
};

RefState reference_chain(int nd, int nm, const std::vector<RefCircuit> &circuits, int tau,
                         bool reset) {
    RefState state;
    state[{0, std::string()}].insert(std::string(static_cast<size_t>(nd + nm), 'I'));

    for (const RefCircuit &c : circuits) {
        std::map<std::string, std::vector<std::set<std::string>>> groups;
        for (const auto &kv : state) {
            auto &level = groups[kv.first.second];
            if (level.empty()) level.resize(static_cast<size_t>(tau) + 1);
            level[static_cast<size_t>(kv.first.first)].insert(kv.second.begin(),
                                                             kv.second.end());
        }

        RefState next;
        for (auto &g : groups) {
            std::vector<std::set<std::string>> by_t = g.second;
            reference_tics(by_t, c.gates, tau);

            std::string prefix = g.first;
            if (!prefix.empty()) prefix.push_back('|');
            for (int t = 0; t <= tau; ++t) {
                for (const auto &p : by_t[static_cast<size_t>(t)]) {
                    const std::string mr = prefix + ref_syndrome(p, nd, c.nm);
                    next[{t, mr}].insert(reset ? ref_reset(p, nd, c.nm) : p);
                }
            }
        }
        state = next;
    }
    return state;
}

void check_against_reference(const std::string &qasm, int nd, int nm,
                             const std::vector<RefGate> &gates, int tau,
                             const char *label, bool reset = true) {
    write_qasm(qasm);
    const auto ref = reference_propagate(nd + nm, gates, tau);
    QasmPropagation p = propagate_qasm(kTmp, tau, reset);

    // Pre-split levels.
    for (int t = 0; t <= tau; ++t) {
        const PauliSetBDD &got = p.at(t);
        assert(got.size() == static_cast<double>(ref[static_cast<size_t>(t)].size()));
        for (const auto &s : ref[static_cast<size_t>(t)]) assert(got.contains(s));
    }

    // Syndrome branches: group the reference by (t, syndrome) and compare.
    std::map<std::pair<int, std::string>, std::set<std::string>> ref_branches;
    for (int t = 0; t <= tau; ++t) {
        for (const auto &s : ref[static_cast<size_t>(t)]) {
            ref_branches[{t, ref_syndrome(s, nd, nm)}].insert(reset ? ref_reset(s, nd, nm)
                                                                     : s);
        }
    }
    assert(p.branches().size() == ref_branches.size());
    for (const SyndromeBranch &b : p.branches()) {
        auto it = ref_branches.find({b.t, b.mr});
        assert(it != ref_branches.end());
        assert(b.set.size() == static_cast<double>(it->second.size()));
        for (const auto &s : it->second) assert(b.set.contains(s));
    }

    std::cout << "    " << label << ": levels and " << p.branches().size()
              << " branches match reference\n";
}

// The branches at one t partition that level exactly -- but only before the
// measurement qubits are reset, since resetting collapses strings together.
void check_partition(const QasmPropagation &p) {
    for (int t = 0; t <= p.tau(); ++t) {
        PauliSetBDD acc   = PauliSetBDD::empty();
        double      total = 0.0;
        for (const SyndromeBranch &b : p.branches()) {
            if (b.t != t) continue;
            assert(!b.set.is_empty());
            assert(static_cast<int>(b.mr.size()) == p.n_measure());
            assert((acc & b.set).is_empty());   // pairwise disjoint
            acc = acc | b.set;
            total += b.set.size();
        }
        assert(acc == p.at(t));                 // union is the whole level
        assert(total == p.at(t).size());
    }
}

// Convenience: the single branch at t = 0 (the fault-free Pauli frame).
const SyndromeBranch &frame_branch(const QasmPropagation &p) {
    for (const SyndromeBranch &b : p.branches()) {
        if (b.t == 0) return b;
    }
    throw std::runtime_error("no t = 0 branch");
}

// Compare a whole PauliFlow chain against the explicit-set reference.
void check_chain(int nd, int nm, const std::vector<std::string> &qasms,
                 const std::vector<RefCircuit> &ref_circuits, int tau, const char *label,
                 bool reset = true) {
    const RefState ref = reference_chain(nd, nm, ref_circuits, tau, reset);

    PauliFlow flow(tau);
    for (size_t i = 0; i < qasms.size(); ++i) {
        const std::string path = std::string("._chain_") + std::to_string(i) + ".qasm";
        std::ofstream(path) << qasms[i];
        flow.run(path, reset);
        std::remove(path.c_str());
    }

    assert(flow.branches().size() == ref.size());
    for (const SyndromeBranch &b : flow.branches()) {
        auto it = ref.find({b.t, b.mr});
        assert(it != ref.end());
        assert(b.set.size() == static_cast<double>(it->second.size()));
        for (const auto &str : it->second) assert(b.set.contains(str));
    }
    std::cout << "    " << label << ": " << flow.branches().size()
              << " branches match reference\n";
}

} // namespace

int main() {
    std::cout << "== parse / validation ==\n";
    const std::string head =
        "OPENQASM 3.0;\ninclude \"stdgates.inc\";\nqubit[1] qd;\nqubit[1] qm;\n";
    assert(rejects(head + "y qd[0];", "unsupported gate y"));
    assert(rejects(head + "ccx qd[0], qm[0];", "unsupported gate ccx"));
    assert(rejects(head + "measure qd[0];", "measure"));
    assert(rejects(head + "bit[1] c;\nc[0] = measure qm[0];", "OQ3 measure assignment"));
    assert(rejects(head + "reset qm[0];", "reset"));
    assert(rejects(head + "h qd;", "register-wide gate"));
    assert(rejects(head + "h qd[5];", "out-of-range index"));
    assert(rejects(head + "cx qd[0], qd[0];", "cx with identical qubits"));
    assert(rejects(head + "h qd[0]", "missing semicolon"));
    assert(rejects("OPENQASM 3.0;\nqubit[1] qd;\nqubit[1] qm;\nqubit[1] qx;",
                   "register not named qd or qm"));
    assert(rejects("OPENQASM 3.0;\nqubit[1] qd;\nqubit[1] qd;", "duplicate qd"));
    assert(rejects("OPENQASM 3.0;\nqubit[2] qd;\nh qd[0];", "missing qm register"));
    assert(rejects("OPENQASM 3.0;\nqubit[2] qm;\nh qm[0];", "missing qd register"));
    assert(rejects("qubit[1] qd;\nqubit[1] qm;", "missing OPENQASM header"));
    assert(rejects("OPENQASM 3.0;\nh qd[0];", "gate before qubit declarations"));
    assert(rejects("OPENQASM 3.0;\ninclude \"other.inc\";\nqubit[1] qd;\nqubit[1] qm;",
                   "unknown include"));

    {
        write_qasm("OPENQASM 3.0;\n// a comment\nqubit[2] qd;\nqubit[1] qm;\n/* block */\n"
                   "barrier qd;\nh qd[0];\nbarrier qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 0);
        assert(p.n_qubits() == 3 && p.n_data() == 2 && p.n_measure() == 1);
        assert(p.n_tics() == 1 && p.n_fault_locations() == 0);
        std::cout << "    barrier + comments accepted and ignored\n";
    }

    std::cout << "\n== register flattening (qd first, then qm) ==\n";
    {
        // A fault location is the only thing that can put a non-identity Pauli
        // into the set -- an x in the circuit is part of the ideal computation
        // and conjugates away -- so the flattening is read off where the fault
        // injected by this cx lands: qd[1] -> 1 and qm[2] -> 2 + 2 = 4.
        write_qasm("OPENQASM 3.0;\nqubit[2] qd;\nqubit[3] qm;\ncx qd[1], qm[2];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1);
        assert(p.data_qubit(0) == 0 && p.data_qubit(1) == 1);
        assert(p.measure_qubit(0) == 2 && p.measure_qubit(2) == 4);
        assert(p.at(1).contains("IXIIX"));
        assert(p.at(1).contains("IZIIZ"));
        assert(!p.at(1).contains("XIIII"));   // nothing lands outside the pair
        std::cout << "    qd[i] -> i, qm[j] -> nd + j\n";
    }

    std::cout << "\n== gate semantics (t = 0 is the Pauli frame) ==\n";
    const std::string h11 = "OPENQASM 3.0;\nqubit[1] qd;\nqubit[1] qm;\n";
    {
        write_qasm(h11 + "x qd[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 0);
        assert(p.at(0).size() == 1.0 && p.at(0).contains("II"));
        std::cout << "    x leaves the frame alone: conjugation by a Pauli is trivial\n";
    }
    {
        write_qasm(h11 + "x qd[0];\nh qd[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 0);
        assert(p.at(0).contains("II"));
        std::cout << "    x then h -> still I; only the h would move a real error\n";
    }
    {
        write_qasm(h11 + "x qd[0];\nx qd[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 0);
        assert(p.at(0).contains("II"));
        std::cout << "    x twice -> I\n";
    }
    {
        // A fault at a cross-register cx lands on exactly the two qubits it
        // touches -- which is also the only way anything non-identity gets
        // into the set, now that a Pauli gate conjugates away.
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1, /*reset_measure=*/false);
        assert(p.n_tics() == 1 && p.n_fault_locations() == 1);
        assert(p.at(1).size() == 16.0);
        assert(p.at(1).contains("XX") && p.at(1).contains("IZ"));
        std::cout << "    cross-register cx is one fault location spanning qd and qm\n";
    }
    {
        // cy is accepted and counts as a fault location like any two-qubit gate.
        write_qasm(h11 + "cy qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1);
        assert(p.n_tics() == 1 && p.n_fault_locations() == 1);
        assert(p.at(1).size() == 16.0);
        std::cout << "    cy is a fault location\n";
    }

    std::cout << "\n== syndrome convention (Z-basis: syndrome = x component) ==\n";
    {
        // One fault location on {qd[0], qm[0]} makes every 2-qubit Pauli
        // reachable, so the branch split has to sort all sixteen by the x
        // component of qm[0] and nothing else.
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1, /*reset_measure=*/false);

        const SyndromeBranch *zero = nullptr;
        const SyndromeBranch *one  = nullptr;
        for (const SyndromeBranch &br : p.branches()) {
            if (br.t != 1) continue;
            if (br.mr == "0") zero = &br; else one = &br;
        }
        assert(zero && one);
        assert(zero->set.size() == 8.0 && one->set.size() == 8.0);

        // X and Y anticommute with Z and flip the outcome; I and Z do not.
        assert(one->set.contains("IX")  && one->set.contains("IY"));
        assert(zero->set.contains("II") && zero->set.contains("IZ"));
        // and the data qubit has no say in it
        assert(zero->set.contains("XI") && zero->set.contains("YI"));
        std::cout << "    syndrome is the x component of qm, and only of qm\n";
    }
    {
        // Two measurement qubits, a fault location on the second one only:
        // the record must read "01", not "10".
        write_qasm("OPENQASM 3.0;\nqubit[1] qd;\nqubit[2] qm;\ncx qd[0], qm[1];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1, /*reset_measure=*/false);
        bool saw = false;
        for (const SyndromeBranch &br : p.branches()) {
            if (br.t == 1 && br.set.contains("IIX")) { assert(br.mr == "01"); saw = true; }
        }
        assert(saw);
        std::cout << "    mr[j] corresponds to qm[j]: X on qm[1] -> \"01\"\n";
    }

    std::cout << "\n== syndrome split ==\n";
    {
        // nd=1, nm=1, one cross-register cx, tau=1.
        //   t=0: {II}          -> qm is I, syndrome "0", 1 string
        //   t=1: all 16        -> "0" holds qm in {I,Z} (8), "1" holds {X,Y} (8)
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1, /*reset_measure=*/false);
        assert(p.at(0).size() == 1.0 && p.at(1).size() == 16.0);
        assert(p.branches().size() == 3);

        assert(p.branches()[0].t == 0 && p.branches()[0].mr == "0");
        assert(p.branches()[0].set.size() == 1.0);
        assert(p.branches()[1].t == 1 && p.branches()[1].mr == "0");
        assert(p.branches()[1].set.size() == 8.0);
        assert(p.branches()[2].t == 1 && p.branches()[2].mr == "1");
        assert(p.branches()[2].set.size() == 8.0);

        // Branch contents respect the convention.
        assert(p.branches()[1].set.contains("XZ") && !p.branches()[1].set.contains("XX"));
        assert(p.branches()[2].set.contains("XX") && !p.branches()[2].set.contains("XZ"));
        check_partition(p);
        std::cout << "    hand-checked case (no reset): 3 branches, sizes 1 / 8 / 8\n";
    }
    {
        // With the default reset the ancilla goes back to I, so each branch
        // keeps only its data content: the 8 strings above collapse to 4.
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1);
        assert(p.branches().size() == 3);
        assert(p.branches()[0].set.size() == 1.0);
        assert(p.branches()[1].set.size() == 4.0);
        assert(p.branches()[2].set.size() == 4.0);

        // Every surviving string carries I on the measurement qubit, and the
        // syndrome now lives only in mr.
        for (const SyndromeBranch &b : p.branches()) {
            b.set.for_each([](const std::string &str) {
                assert(str[1] == 'I');
                return true;
            });
        }
        // Both syndromes now hold the same data content -- they are told apart
        // by mr, not by the set.
        assert(p.branches()[1].set == p.branches()[2].set);
        std::cout << "    reset (default): qm back to I, sizes 1 / 4 / 4\n";
    }
    {
        // Empty branches are dropped: t = 0 is a single Pauli, so exactly one
        // of the 2^nm syndromes is reachable however large nm is.
        write_qasm("OPENQASM 3.0;\nqubit[1] qd;\nqubit[4] qm;\nh qd[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 2);
        assert(p.branches().size() == 1);            // not 2^4 = 16
        assert(p.branches()[0].t == 0 && p.branches()[0].mr == "0000");
        std::cout << "    unreachable syndromes are dropped (1 branch, not 16)\n";
    }
    {
        // Branches stay ordered by (t, mr).
        write_qasm("OPENQASM 3.0;\nqubit[2] qd;\nqubit[2] qm;\n"
                   "cx qd[0], qm[0];\ncx qd[1], qm[1];\ncz qd[0], qd[1];\n");
        QasmPropagation p = propagate_qasm(kTmp, 2, /*reset_measure=*/false);
        for (size_t i = 1; i < p.branches().size(); ++i) {
            const auto &a = p.branches()[i - 1];
            const auto &b = p.branches()[i];
            assert(a.t < b.t || (a.t == b.t && a.mr < b.mr));
        }
        check_partition(p);
        std::cout << "    branches ordered by (t, mr); partition holds at every t\n";
    }

    std::cout << "\n== agreement with an explicit-set reference implementation ==\n";
    check_against_reference(
        "OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\n"
        "h qd[0];\ncx qd[0], qm[0];\nx qd[1];\ncz qd[1], qm[0];\n",
        2, 1, {{"h", 0, 0}, {"cx", 0, 2}, {"x", 1, 0}, {"cz", 1, 2}}, 2,
        "2+1 qubits / 4 gates / tau=2");
    check_against_reference(
        "OPENQASM 3.0;\nqubit[2] qd;\nqubit[2] qm;\n"
        "cx qd[0], qm[0];\nh qd[1];\ncz qd[1], qm[1];\nz qd[0];\ncx qm[0], qm[1];\n",
        2, 2, {{"cx", 0, 2}, {"h", 1, 0}, {"cz", 1, 3}, {"z", 0, 0}, {"cx", 2, 3}}, 2,
        "2+2 qubits / 5 gates / tau=2");
    check_against_reference(
        "OPENQASM 3.0;\nqubit[1] qd;\nqubit[2] qm;\n"
        "cx qd[0], qm[0];\ncx qd[0], qm[1];\ncz qm[0], qm[1];\n",
        1, 2, {{"cx", 0, 1}, {"cx", 0, 2}, {"cz", 1, 2}}, 3,
        "1+2 qubits / 3 fault locations / tau=3");
    check_against_reference(
        "OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\n"
        "cy qd[0], qm[0];\nh qd[1];\ncy qd[1], qd[0];\ncx qd[1], qm[0];\n",
        2, 1, {{"cy", 0, 2}, {"h", 1, 0}, {"cy", 1, 0}, {"cx", 1, 2}}, 2,
        "2+1 qubits / cy mixed with cx / tau=2");
    check_against_reference(
        "OPENQASM 3.0;\nqubit[1] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\nh qd[0];\n", 1, 1,
        {{"cx", 0, 1}, {"h", 0, 0}}, 0, "tau=0 never faults");

    std::cout << "\n== fault-count bookkeeping ==\n";
    {
        write_qasm("OPENQASM 3.0;\nqubit[1] qd;\nqubit[1] qm;\nh qd[0];\nx qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 2);
        assert(p.n_tics() == 2 && p.n_fault_locations() == 0);
        assert(p.at(1).is_empty() && p.at(2).is_empty());
        assert(p.branches().size() == 1);
        std::cout << "    no cx/cz: only t = 0 is populated\n";
    }
    {
        write_qasm("OPENQASM 3.0;\nqubit[1] qd;\nqubit[2] qm;\n"
                   "cx qd[0], qm[0];\ncz qm[0], qm[1];\n");
        QasmPropagation p = propagate_qasm(kTmp, 4, /*reset_measure=*/false);
        assert(p.n_fault_locations() == 2);
        assert(!p.at(1).is_empty() && !p.at(2).is_empty());
        assert(p.at(3).is_empty() && p.at(4).is_empty());
        assert((p.at(0) - p.at(1)).is_empty());   // levels nest while reachable
        assert((p.at(1) - p.at(2)).is_empty());
        check_partition(p);
        std::cout << "    tau > fault locations: upper levels empty; levels nested\n";
    }

    std::cout << "\n== chaining circuits (PauliFlow) ==\n";
    const std::string chain_a = "OPENQASM 3.0;\nqubit[1] qd;\nqubit[1] qm;\n"
                                "cx qd[0], qm[0];\n";
    {
        // One round through PauliFlow must equal propagate_qasm on its own.
        write_qasm(chain_a);
        PauliFlow flow(1);
        flow.run(kTmp);
        assert(flow.n_rounds() == 1 && flow.n_data() == 1 && flow.n_measure() == 1);
        assert(flow.branches().size() == 3);
        assert(flow.branches()[0].mr == "0" && flow.branches()[0].set.size() == 1.0);
        assert(flow.branches()[1].set.size() == 4.0);
        assert(flow.branches()[2].set.size() == 4.0);
        std::cout << "    single round matches the standalone propagation\n";
    }
    {
        // Two rounds: records get a separator, and only the data survives.
        write_qasm(chain_a);
        PauliFlow flow(1);
        flow.run(kTmp);
        flow.run(kTmp);
        assert(flow.n_rounds() == 2);
        for (const SyndromeBranch &b : flow.branches()) {
            assert(b.mr.size() == 3 && b.mr[1] == PauliFlow::kRoundSeparator);
            b.set.for_each([](const std::string &str) {
                assert(str[1] == 'I');   // the ancilla is fresh again
                return true;
            });
        }
        // (0,"0|0") is the only fault-free path; every other branch is at t=1.
        int zero_fault = 0;
        for (const SyndromeBranch &b : flow.branches()) {
            if (b.t == 0) { ++zero_fault; assert(b.mr == "0|0"); }
        }
        assert(zero_fault == 1);
        std::cout << "    two rounds: mr = \"s|s\", ancilla reset between them\n";
    }
    {
        // The budget spans the chain: with tau = 0 nothing ever faults, so a
        // single fault-free path survives however many rounds run.
        write_qasm(chain_a);
        PauliFlow flow(0);
        flow.run(kTmp);
        flow.run(kTmp);
        flow.run(kTmp);
        assert(flow.branches().size() == 1);
        assert(flow.branches()[0].t == 0 && flow.branches()[0].mr == "0|0|0");
        std::cout << "    tau = 0 over three rounds: one path, no faults\n";
    }
    {
        // nd must not change between circuits.
        write_qasm(chain_a);
        PauliFlow flow(1);
        flow.run(kTmp);
        write_qasm("OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\ncx qd[0], qm[0];\n");
        bool threw = false;
        try { flow.run(kTmp); } catch (const std::runtime_error &e) {
            threw = true;
            std::cout << "    rejected a data-width change: " << e.what() << "\n";
        }
        assert(threw);
    }
    {
        // A later round may use more ancillas; the space grows and the new
        // slots start at identity rather than opening up 4^added patterns.
        write_qasm(chain_a);
        PauliFlow flow(1);
        flow.run(kTmp);
        assert(flow.n_measure() == 1);

        write_qasm("OPENQASM 3.0;\nqubit[1] qd;\nqubit[3] qm;\n"
                   "cx qd[0], qm[0];\ncx qd[0], qm[2];\n");
        flow.run(kTmp);
        assert(flow.n_measure() == 3 && flow.n_qubits() == 4);
        for (const SyndromeBranch &b : flow.branches()) {
            assert(b.mr.size() == 5);   // "s|sss"
            b.set.for_each([](const std::string &str) {
                assert(str.size() == 4);
                assert(str[1] == 'I' && str[2] == 'I' && str[3] == 'I');
                return true;
            });
        }
        std::cout << "    a wider round grows the ancilla space (nm 1 -> 3)\n";
    }

    // Full agreement with the reference, chained.
    check_chain(1, 1, {chain_a, chain_a},
                {{1, {{"cx", 0, 1}}}, {1, {{"cx", 0, 1}}}}, 2, "1+1 x 2 rounds / tau=2");
    check_chain(2, 1,
                {"OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\n"
                 "cx qd[0], qm[0];\ncx qd[1], qm[0];\n",
                 "OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\n"
                 "h qd[0];\ncx qd[1], qm[0];\ncz qd[0], qd[1];\n"},
                {{1, {{"cx", 0, 2}, {"cx", 1, 2}}},
                 {1, {{"h", 0, 0}, {"cx", 1, 2}, {"cz", 0, 1}}}},
                2, "2+1 x 2 rounds / tau=2");
    check_chain(2, 1,
                {"OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\n"
                 "cy qd[0], qm[0];\ncx qd[1], qm[0];\n",
                 "OPENQASM 3.0;\nqubit[2] qd;\nqubit[1] qm;\n"
                 "h qd[0];\ncy qd[1], qm[0];\n"},
                {{1, {{"cy", 0, 2}, {"cx", 1, 2}}}, {1, {{"h", 0, 0}, {"cy", 1, 2}}}},
                2, "2+1 x 2 rounds with cy / tau=2");
    check_chain(1, 2,
                {"OPENQASM 3.0;\nqubit[1] qd;\nqubit[2] qm;\n"
                 "cx qd[0], qm[0];\ncx qm[0], qm[1];\n",
                 "OPENQASM 3.0;\nqubit[1] qd;\nqubit[2] qm;\n"
                 "h qm[1];\ncx qd[0], qm[1];\nh qm[1];\n"},
                {{2, {{"cx", 0, 1}, {"cx", 1, 2}}},
                 {2, {{"h", 2, 0}, {"cx", 0, 2}, {"h", 2, 0}}}},
                2, "1+2 x 2 rounds / tau=2 (flag-style)");
    check_chain(1, 1, {chain_a, chain_a},
                {{1, {{"cx", 0, 1}}}, {1, {{"cx", 0, 1}}}}, 1,
                "1+1 x 2 rounds / tau=1, no reset", /*reset=*/false);

    std::cout << "\n== session ownership ==\n";
    {
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        { QasmPropagation a = propagate_qasm(kTmp, 1); assert(a.at(1).size() == 16.0); }
        { QasmPropagation b = propagate_qasm(kTmp, 1); assert(b.at(1).size() == 16.0); }
        std::cout << "    two sequential sessions ok\n";
    }
    {
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        QasmPropagation a = propagate_qasm(kTmp, 1);
        QasmPropagation b = std::move(a);
        assert(b.at(1).size() == 16.0 && b.branches().size() == 3);
        std::cout << "    move transfers session ownership\n";
    }
    {
        assert(rejects(h11 + "y qd[0];", "gate after a valid header"));
        write_qasm(h11 + "cx qd[0], qm[0];\n");
        QasmPropagation p = propagate_qasm(kTmp, 1);
        assert(p.at(1).size() == 16.0);
        std::cout << "    a failed parse leaves no dangling session\n";
    }

    std::remove(kTmp);
    std::cout << "\nAll QASM propagation tests passed.\n";
    return 0;
}
