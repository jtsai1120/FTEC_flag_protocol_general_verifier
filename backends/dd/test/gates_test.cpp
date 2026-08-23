#include "pauli_bdd.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace pbdd;

int main() {
    const int n = 2;
    PauliSetBDD::init(n);

    // --- apply_X / apply_Z: composition (Pauli-group multiplication),
    // NOT conjugation. Flips exactly one of the two bits on qubit q.
    //   apply_X(q): (x,z) -> (x^1, z)      I<->X, Z<->Y
    //   apply_Z(q): (x,z) -> (x, z^1)      I<->Z, X<->Y
    assert(PauliSetBDD::single("II").apply_X(0) == PauliSetBDD::single("XI"));
    assert(PauliSetBDD::single("XI").apply_X(0) == PauliSetBDD::single("II"));
    assert(PauliSetBDD::single("ZI").apply_X(0) == PauliSetBDD::single("YI"));
    assert(PauliSetBDD::single("YI").apply_X(0) == PauliSetBDD::single("ZI"));
    assert(PauliSetBDD::single("IX").apply_X(1) == PauliSetBDD::single("II"));

    assert(PauliSetBDD::single("II").apply_Z(0) == PauliSetBDD::single("ZI"));
    assert(PauliSetBDD::single("XI").apply_Z(0) == PauliSetBDD::single("YI"));
    assert(PauliSetBDD::single("ZI").apply_Z(0) == PauliSetBDD::single("II"));
    assert(PauliSetBDD::single("YI").apply_Z(0) == PauliSetBDD::single("XI"));
    std::cout << "apply_X / apply_Z composition tests passed\n";

    // X, Z applied twice is the identity map (each is its own inverse).
    assert(PauliSetBDD::single("YZ").apply_X(0).apply_X(0) == PauliSetBDD::single("YZ"));
    assert(PauliSetBDD::single("YZ").apply_Z(1).apply_Z(1) == PauliSetBDD::single("YZ"));
    std::cout << "apply_X / apply_Z involution tests passed\n";

    // --- apply_H: conjugation, (x_q,z_q) -> (z_q,x_q) swap.
    assert(PauliSetBDD::single("II").apply_H(0) == PauliSetBDD::single("II"));
    assert(PauliSetBDD::single("XI").apply_H(0) == PauliSetBDD::single("ZI"));
    assert(PauliSetBDD::single("ZI").apply_H(0) == PauliSetBDD::single("XI"));
    assert(PauliSetBDD::single("YI").apply_H(0) == PauliSetBDD::single("YI"));
    assert(PauliSetBDD::single("IX").apply_H(1) == PauliSetBDD::single("IZ"));
    assert(PauliSetBDD::single("XI").apply_H(0).apply_H(0) == PauliSetBDD::single("XI"));
    std::cout << "apply_H tests passed\n";

    // --- apply_S: conjugation, (x,z) -> (x, x^z). X -> Y -> X, Z fixed.
    assert(PauliSetBDD::single("II").apply_S(0) == PauliSetBDD::single("II"));
    assert(PauliSetBDD::single("XI").apply_S(0) == PauliSetBDD::single("YI"));
    assert(PauliSetBDD::single("YI").apply_S(0) == PauliSetBDD::single("XI"));
    assert(PauliSetBDD::single("ZI").apply_S(0) == PauliSetBDD::single("ZI"));
    assert(PauliSetBDD::single("IX").apply_S(1) == PauliSetBDD::single("IY"));
    // Involution here too, for a different reason than the rest: S^2 = Z, and
    // conjugating by a Pauli leaves the type alone.
    assert(PauliSetBDD::single("XY").apply_S(0).apply_S(0) == PauliSetBDD::single("XY"));
    std::cout << "apply_S tests passed\n";

    // --- apply_CX(control=0, target=1): conjugation,
    //   x_t' = x_t ^ x_c,  z_c' = z_c ^ z_t,  x_c'/z_t' unchanged.
    assert(PauliSetBDD::single("XI").apply_CX(0, 1) == PauliSetBDD::single("XX"));
    assert(PauliSetBDD::single("IZ").apply_CX(0, 1) == PauliSetBDD::single("ZZ"));
    assert(PauliSetBDD::single("ZI").apply_CX(0, 1) == PauliSetBDD::single("ZI")); // Z_c unchanged
    assert(PauliSetBDD::single("IX").apply_CX(0, 1) == PauliSetBDD::single("IX")); // X_t unchanged
    assert(PauliSetBDD::single("YZ").apply_CX(0, 1).apply_CX(0, 1) == PauliSetBDD::single("YZ")); // involution
    std::cout << "apply_CX tests passed\n";

    // --- apply_CZ(control=0, target=1): conjugation,
    //   z_c' = z_c ^ x_t,  z_t' = z_t ^ x_c,  x_c'/x_t' unchanged.
    assert(PauliSetBDD::single("XI").apply_CZ(0, 1) == PauliSetBDD::single("XZ"));
    assert(PauliSetBDD::single("IX").apply_CZ(0, 1) == PauliSetBDD::single("ZX"));
    assert(PauliSetBDD::single("ZI").apply_CZ(0, 1) == PauliSetBDD::single("ZI")); // Z_c unchanged
    assert(PauliSetBDD::single("IZ").apply_CZ(0, 1) == PauliSetBDD::single("IZ")); // Z_t unchanged
    assert(PauliSetBDD::single("YZ").apply_CZ(0, 1).apply_CZ(0, 1) == PauliSetBDD::single("YZ")); // involution
    std::cout << "apply_CZ tests passed\n";

    // --- apply_CY(control=0, target=1): conjugation,
    //   z_c' = z_c ^ x_t ^ z_t,  x_t' = x_t ^ x_c,  z_t' = z_t ^ x_c,  x_c fixed.
    // Generator images: X_c -> X_c Y_t, Z_c -> Z_c, X_t -> Z_c X_t, Z_t -> Z_c Z_t.
    assert(PauliSetBDD::single("XI").apply_CY(0, 1) == PauliSetBDD::single("XY"));
    assert(PauliSetBDD::single("ZI").apply_CY(0, 1) == PauliSetBDD::single("ZI")); // Z_c fixed
    assert(PauliSetBDD::single("IX").apply_CY(0, 1) == PauliSetBDD::single("ZX"));
    assert(PauliSetBDD::single("IZ").apply_CY(0, 1) == PauliSetBDD::single("ZZ"));
    // Y_t commutes with CY, so it comes back untouched.
    assert(PauliSetBDD::single("IY").apply_CY(0, 1) == PauliSetBDD::single("IY"));
    assert(PauliSetBDD::single("YI").apply_CY(0, 1) == PauliSetBDD::single("YY"));
    assert(PauliSetBDD::single("XX").apply_CY(0, 1) == PauliSetBDD::single("YZ"));
    assert(PauliSetBDD::single("II").apply_CY(0, 1) == PauliSetBDD::single("II"));
    assert(PauliSetBDD::single("YZ").apply_CY(0, 1).apply_CY(0, 1)
           == PauliSetBDD::single("YZ"));   // involution, since Y^2 = I
    std::cout << "apply_CY tests passed\n";

    // Independent cross-check: CY = (I@S) CX (I@S^), and phase-free S maps
    // (x,z) -> (x, x^z) on its qubit, so conjugating by CY must equal S_t then
    // CX then S_t. Build S_t out of the primitives we already trust.
    {
        auto s_on_target = [](const PauliSetBDD &in, int q) {
            // z_q := z_q ^ x_q, x_q unchanged.
            bddPair *pr = bdd_newpair();
            bdd_setbddpair(pr, PauliSetBDD::zvar(q),
                           bdd_ithvar(PauliSetBDD::zvar(q)) ^ bdd_ithvar(PauliSetBDD::xvar(q)));
            bdd out = bdd_veccompose(in.raw(), pr);
            bdd_freepair(pr);
            return PauliSetBDD(out);
        };
        for (int p0 = 0; p0 < 4; ++p0) {
            for (int p1 = 0; p1 < 4; ++p1) {
                std::string str;
                str += PauliSetBDD::pauli_to_char(static_cast<Pauli>(p0));
                str += PauliSetBDD::pauli_to_char(static_cast<Pauli>(p1));
                const PauliSetBDD in = PauliSetBDD::single(str);
                const PauliSetBDD via_decomposition =
                    s_on_target(s_on_target(in, 1).apply_CX(0, 1), 1);
                assert(in.apply_CY(0, 1) == via_decomposition);
            }
        }
        std::cout << "apply_CY matches the S.CX.S decomposition on all 16 Paulis\n";
    }

    // --- gates act correctly on multi-element sets, not just singletons.
    PauliSetBDD s = PauliSetBDD::single("XI") | PauliSetBDD::single("IZ");
    PauliSetBDD s_cx = s.apply_CX(0, 1);
    assert(s_cx.size() == 2.0);
    assert(s_cx.contains("XX"));
    assert(s_cx.contains("ZZ"));

    PauliSetBDD s_cy = s.apply_CY(0, 1);
    assert(s_cy.size() == 2.0);
    assert(s_cy.contains("XY"));
    assert(s_cy.contains("ZZ"));
    std::cout << "gate-on-set tests passed\n";

    // --- error handling: out-of-range qubit indices / degenerate CX/CZ.
    bool threw = false;
    try { PauliSetBDD::single("II").apply_H(5); } catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { PauliSetBDD::single("II").apply_CX(0, 0); } catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { PauliSetBDD::single("II").apply_CZ(-1, 1); } catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { PauliSetBDD::single("II").apply_CY(0, 0); } catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { PauliSetBDD::single("II").apply_CY(0, 7); } catch (const std::invalid_argument &) { threw = true; }
    assert(threw);

    threw = false;
    try { PauliSetBDD::single("II").apply_S(9); } catch (const std::invalid_argument &) { threw = true; }
    assert(threw);
    std::cout << "error handling tests passed\n";

    std::cout << "\nAll gate tests passed.\n";

    PauliSetBDD::done();
    return 0;
}
