#include "pauli_bdd.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

using namespace pbdd;

namespace {

// Reference implementation of "multiply by every Pauli supported on {a,b}",
// written the naive way the fast path deliberately avoids: build all 16
// two-qubit Paulis explicitly, multiply each onto the whole set, union the
// results. Multiplying by a single-qubit Pauli p reduces to the already
// tested apply_X / apply_Z, since those *are* per-bit composition.
PauliSetBDD multiply_one_qubit(const PauliSetBDD &s, int q, Pauli p) {
    const uint8_t v = static_cast<uint8_t>(p);
    PauliSetBDD out = s;
    if (v & 0x1) out = out.apply_X(q);
    if (v & 0x2) out = out.apply_Z(q);
    return out;
}

PauliSetBDD naive_multiply_all_on(const PauliSetBDD &s, int a, int b) {
    PauliSetBDD acc = PauliSetBDD::empty();
    for (int pa = 0; pa < 4; ++pa) {
        for (int pb = 0; pb < 4; ++pb) {
            PauliSetBDD term = multiply_one_qubit(s, a, static_cast<Pauli>(pa));
            term = multiply_one_qubit(term, b, static_cast<Pauli>(pb));
            acc = acc | term;
        }
    }
    return acc;
}

} // namespace

int main() {
    const int n = 4;
    PauliSetBDD::init(n);

    // --- concrete values -------------------------------------------------
    // Forgetting qubits 0,1 of {XIII} leaves qubits 2,3 pinned to I and lets
    // qubits 0,1 range over all 4x4 combinations.
    PauliSetBDD s1 = PauliSetBDD::single("XIII");
    PauliSetBDD r1 = s1.multiply_by_all_paulis_on(0, 1);
    assert(r1.size() == 16.0);
    assert(r1.contains("YZII"));
    assert(r1.contains("XIII"));   // identity is among the 16 multipliers
    assert(!r1.contains("IIXI"));  // qubit 2 must still be I
    std::cout << "concrete-value tests passed (size = " << r1.size() << ")\n";

    // Single-qubit subset: 4 strings, not 16.
    PauliSetBDD r_one = s1.multiply_by_all_paulis_on(std::vector<int>{0});
    assert(r_one.size() == 4.0);
    assert(r_one.contains("IIII") && r_one.contains("YIII"));
    assert(!r_one.contains("IXII"));
    std::cout << "single-qubit-subset test passed\n";

    // --- structural properties ------------------------------------------
    PauliSetBDD s = PauliSetBDD::single("XYZI") | PauliSetBDD::single("IIZZ")
                  | PauliSetBDD::single("ZZII");

    PauliSetBDD r = s.multiply_by_all_paulis_on(0, 1);

    // Idempotent: quantifying the same variables twice changes nothing.
    assert(r.multiply_by_all_paulis_on(0, 1) == r);

    // Superset: the identity multiplier keeps every original string.
    assert((s - r).is_empty());

    // Every surviving "outside class" contributes exactly 16 strings.
    assert(std::fmod(r.size(), 16.0) == 0.0);

    // Disjoint qubit pairs commute.
    assert(s.multiply_by_all_paulis_on(0, 1).multiply_by_all_paulis_on(2, 3)
           == s.multiply_by_all_paulis_on(2, 3).multiply_by_all_paulis_on(0, 1));

    // Forgetting every qubit yields the full 4^n universe.
    assert(s.multiply_by_all_paulis_on(std::vector<int>{0, 1, 2, 3})
           == PauliSetBDD::universe());
    std::cout << "structural-property tests passed\n";

    // Edge cases: empty stays empty, universe stays universe.
    assert(PauliSetBDD::empty().multiply_by_all_paulis_on(0, 1).is_empty());
    assert(PauliSetBDD::universe().multiply_by_all_paulis_on(0, 1)
           == PauliSetBDD::universe());
    std::cout << "edge-case tests passed\n";

    // --- agreement with the naive 16-multiply-and-union reference --------
    assert(r == naive_multiply_all_on(s, 0, 1));
    assert(r1 == naive_multiply_all_on(s1, 0, 1));

    std::mt19937 rng(2024);
    std::uniform_int_distribution<int> pick(0, 3);
    for (int trial = 0; trial < 20; ++trial) {
        PauliSetBDD rnd = PauliSetBDD::empty();
        for (int k = 0; k < 6; ++k) {
            std::string str;
            for (int q = 0; q < n; ++q) {
                str += PauliSetBDD::pauli_to_char(static_cast<Pauli>(pick(rng)));
            }
            rnd = rnd | PauliSetBDD::single(str);
        }
        for (int a = 0; a < n; ++a) {
            for (int b = 0; b < n; ++b) {
                if (a == b) continue;
                assert(rnd.multiply_by_all_paulis_on(a, b)
                       == naive_multiply_all_on(rnd, a, b));
            }
        }
    }
    std::cout << "randomized agreement with naive reference passed "
                 "(20 sets x 12 qubit pairs)\n";

    // --- the point of the exercise: the result is not bigger -------------
    std::cout << "node_count: before = " << s.node_count()
              << ", after = " << r.node_count() << "\n";
    assert(r.node_count() <= s.node_count());

    // --- error handling ---------------------------------------------------
    bool threw = false;
    try { s.multiply_by_all_paulis_on(1, 1); }
    catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { s.multiply_by_all_paulis_on(std::vector<int>{0, 0}); }
    catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { s.multiply_by_all_paulis_on(0, n); }
    catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    // The empty subset multiplies by {identity} only, so it is a no-op.
    assert(s.multiply_by_all_paulis_on(std::vector<int>{}) == s);
    std::cout << "error handling / empty-subset tests passed\n";

    std::cout << "\n== reset_qubits ==\n";
    {
        // Forget and pin back to identity.
        assert(PauliSetBDD::single("XYZI").reset_qubits(std::vector<int>{2, 3})
               == PauliSetBDD::single("XYII"));

        // It is a projection: strings differing only on the reset qubits merge.
        const PauliSetBDD pair =
            PauliSetBDD::single("XYZI") | PauliSetBDD::single("XYIX");
        assert(pair.size() == 2.0);
        assert(pair.reset_qubits(std::vector<int>{2, 3}) == PauliSetBDD::single("XYII"));

        // Unlike a bare forget, the set does not blow up by 4^|qubits|.
        assert(PauliSetBDD::single("XYZI").multiply_by_all_paulis_on(2, 3).size() == 16.0);
        assert(PauliSetBDD::single("XYZI").reset_qubits(std::vector<int>{2, 3}).size() == 1.0);

        // Idempotent, and an empty list is a no-op.
        const PauliSetBDD r = s.reset_qubits(std::vector<int>{2, 3});
        assert(r.reset_qubits(std::vector<int>{2, 3}) == r);
        assert(s.reset_qubits(std::vector<int>{}) == s);

        // Everything it produces really is I on those qubits.
        r.for_each([](const std::string &str) {
            assert(str[2] == 'I' && str[3] == 'I');
            return true;
        });
        assert(PauliSetBDD::empty().reset_qubits(std::vector<int>{0}).is_empty());
        std::cout << "    reset_qubits: forgets, pins to I, and merges accordingly\n";
    }

    std::cout << "\n== grow ==\n";
    {
        // Run this last: it changes the package-wide qubit count.
        const PauliSetBDD before = PauliSetBDD::single("XYZI");
        assert(before.size() == 1.0);

        PauliSetBDD::grow(5);
        assert(PauliSetBDD::num_qubits() == 5);

        // The old BDD is unchanged as a *function*, so it now leaves the new
        // qubit free -- exactly the hazard grow() documents. size() only
        // reports that because grow() flushes BuDDy's operator cache; without
        // that, the satcount computed above would be handed back stale.
        assert(before.size() == 4.0);
        assert(before.contains("XYZIX"));

        const PauliSetBDD pinned = before.reset_qubits(std::vector<int>{4});
        assert(pinned.size() == 1.0 && pinned.contains("XYZII"));

        // Growing does not disturb existing variables, and the new one takes
        // part in reordering like the rest.
        bdd_reorder(BDD_REORDER_SIFT);
        assert(pinned.contains("XYZII") && !pinned.contains("XYZIX"));

        bool threw = false;
        try { PauliSetBDD::grow(4); } catch (const std::invalid_argument &) { threw = true; }
        assert(threw);
        assert(PauliSetBDD::num_qubits() == 5);
        std::cout << "    grow: adds qubits, old sets gain free variables until pinned\n";
    }

    std::cout << "\nAll subgroup-multiplication tests passed.\n";

    PauliSetBDD::done();
    return 0;
}
