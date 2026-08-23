#pragma once

#include "pauli_bdd.hpp"

#include <string>
#include <vector>

namespace pbdd {

// Two errors drawn from the same set whose product is a non-trivial logical
// operator -- the code cannot tell them apart, and correcting one damages the
// other. `found` false means no such pair exists and every other field is empty.
struct LogicalCollision {
    bool        found = false;
    std::string syndrome;    // k bits; syndrome[i] = <g_i, E>, shared by both
    std::string logical_a;   // 2m bits; logical signature of witness_1
    std::string logical_b;   // 2m bits; differs from logical_a
    std::string witness_1;   // Pauli string over the n_data data qubits
    std::string witness_2;   // witness_1 * witness_2 lies in N(S) \ S
};

// A stabilizer code, given by generators over the data qubits.
//
// Construction does the one-off linear algebra: it checks the generators are
// independent and pairwise commuting, then completes them (symplectic
// Gram-Schmidt) to a full symplectic basis of GF(2)^(2*n_data)
//
//     g_1..g_k   stabilizers          d_1..d_k   destabilizers
//     X_1..X_m   logical X            Z_1..Z_m   logical Z          m = n_data - k
//
// with <g_i,d_j> = delta_ij, <X_i,Z_j> = delta_ij and every other pairing zero.
// In the coordinates that basis induces,
//
//     a_i = <E,d_i>   stabilizer component
//     b_i = <E,g_i>   syndrome
//     c_j = <E,Z_j>   \  logical signature
//     f_j = <E,X_j>   /
//
// the stabilizer group S is simply "b = c = f = 0" -- an axis-aligned subspace.
// That is the whole point: S is not axis-aligned in the original coordinates,
// so quotienting by it needs a relation; after the change of basis it is one
// bdd_exist, exactly like PauliSetBDD::multiply_by_all_paulis_on.
//
// Phases are ignored throughout, matching PauliSetBDD. That is the right call
// here: E1*E2 = ±g for a stabilizer g means the same recovery fixes both.
class StabilizerCode {
public:
    // Generators are Pauli strings of length n_data, e.g. {"IIIXXXX", ...}.
    // Throws std::invalid_argument if they do not pairwise commute, are not
    // linearly independent, or leave no logical qubits (m must be > 0).
    StabilizerCode(int n_data, const std::vector<std::string> &generators);

    int n_data() const { return n_; }
    int k() const { return k_; }        // number of generators
    int m() const { return m_; }        // logical qubits, n_data - k

    // Syndrome of one Pauli string: k bits, bit i = <g_i, P>. Zero exactly
    // when P commutes with every generator, i.e. P is in N(S).
    std::string syndrome_of(const std::string &pauli) const;

    // The logical representatives this object *chose*. They are not supplied
    // by the caller and they are not unique: multiplying any of them by a
    // stabilizer gives an equally valid representative of the same class. What
    // is canonical is the partition of N(S) into classes -- "logical_of(e) is
    // zero" means exactly "e is in S" whatever representatives were picked --
    // so a verdict never depends on this choice, only the 2m-bit *labels* do.
    std::string logical_x(int j) const;
    std::string logical_z(int j) const;
    std::string destabilizer(int i) const;

    // Logical signature: 2m bits, (<P,Z_j>, <P,X_j>) per logical qubit. For a
    // P in N(S) this is zero exactly when P is in S, so a P in N(S) is a
    // non-trivial logical operator precisely when this is non-zero.
    std::string logical_of(const std::string &pauli) const;

private:
    friend LogicalCollision find_undetectable_logical_pair(const StabilizerCode &,
                                                           const PauliSetBDD &);

    // GF(2) vectors of length 2n, laid out like the BDD variables:
    // index 2q is the x component of qubit q, 2q+1 the z component.
    using Vec = std::vector<unsigned char>;

    int              n_ = 0, k_ = 0, m_ = 0;
    std::vector<Vec> g_, d_, x_, z_;
    std::vector<Vec> tinv_;   // e = tinv_ * y, the substitution for veccompose

    Vec parse(const std::string &pauli, const char *who) const;
};

// Does the set contain two elements whose product is a non-trivial logical
// operator? Equivalently: two elements sharing a fault-free full syndrome but
// carrying different logical signatures.
//
// Both maps are GF(2)-linear, so for E1, E2 with vectors v1, v2:
//     E1*E2 in N(S)  <=>  syndrome(v1) = syndrome(v2)
// and, given that, E1*E2 in S <=> logical(v1) = logical(v2). Two errors with
// *different* syndromes therefore never produce an element of N(S) at all --
// the same-syndrome case is not a restriction, it is the whole problem.
//
// The set may be defined over more qubits than the code (the measurement
// register); those variables are projected away first, since the stabilizers
// only act on the data qubits.
LogicalCollision find_undetectable_logical_pair(const StabilizerCode &code,
                                                const PauliSetBDD &set);

inline bool has_undetectable_logical_pair(const StabilizerCode &code,
                                          const PauliSetBDD &set) {
    return find_undetectable_logical_pair(code, set).found;
}

} // namespace pbdd
