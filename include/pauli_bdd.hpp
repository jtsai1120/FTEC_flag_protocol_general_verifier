#pragma once

#include <bdd.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pbdd {

// Single-qubit Pauli label. The numeric encoding matches the symplectic
// (X,Z) bit pair directly:
//   bit0 (LSB) = 1  iff an X component is present
//   bit1       = 1  iff a Z component is present
// I=00, X=01, Z=10, Y=11   (Y == XZ up to a phase, which we do not track)
enum class Pauli : uint8_t { I = 0, X = 1, Z = 2, Y = 3 };

// A set of n-qubit Pauli strings (phase/sign ignored), represented as a BDD
// over 2n boolean variables x_0,z_0,x_1,z_1,...,x_{n-1},z_{n-1} (interleaved
// at declaration time; BuDDy's dynamic reordering is free to permute the
// internal level order afterwards -- variable *identity*, which is all this
// class relies on, never changes).
//
// BuDDy's package is a process-wide singleton, so PauliSetBDD::init() must
// be called exactly once before constructing any PauliSetBDD, and
// PauliSetBDD::done() once you are completely finished with all of them.
class PauliSetBDD {
public:
    // --- package lifecycle (call once, globally) ----------------------
    static void init(int n_qubits, int node_num = 100000, int cache_size = 10000);
    static void done();
    static int  num_qubits() { return n_; }

    // Raise the qubit count without tearing the package down. BuDDy can only
    // ever add variables, so this never shrinks; passing the current count is
    // a no-op and a smaller one throws.
    //
    // CAUTION: every existing PauliSetBDD keeps its meaning as a *function*,
    // which means it does not constrain the new variables at all -- as a set
    // it silently gains all 4^added combinations on the new qubits. Pin them
    // with reset_qubits() right after growing unless that is what you want.
    static void grow(int n_qubits);

    // Fixed variable-identity convention. bdd_var() on a node always
    // returns one of these regardless of how reordering has rearranged
    // the internal level order.
    static int xvar(int q) { return 2 * q; }
    static int zvar(int q) { return 2 * q + 1; }

    // --- construction ---------------------------------------------------
    static PauliSetBDD empty();                                   // {}
    static PauliSetBDD universe();                                 // all 4^n strings
    static PauliSetBDD single(const std::vector<Pauli> &paulis);   // {P}
    static PauliSetBDD single(const std::string &paulis);          // e.g. "IXYZ"

    // Wrap an already-built bdd (escape hatch for future gate code).
    explicit PauliSetBDD(bdd f) : f_(f) {}

    // --- set algebra ------------------------------------------------------
    PauliSetBDD operator|(const PauliSetBDD &rhs) const;  // union
    PauliSetBDD operator&(const PauliSetBDD &rhs) const;  // intersection
    PauliSetBDD operator-(const PauliSetBDD &rhs) const;  // set difference
    PauliSetBDD operator!() const;                         // complement (within the 4^n universe)

    // --- gates ----------------------------------------------------------
    // Every string in the set is transformed independently; the returned
    // set is the image of *this under the gate. See README.md "5 個 Gate"
    // for the two distinct semantics in play:
    //   - X, Z are themselves Pauli-group elements: applying them means
    //     *composing* (group-multiplying, phase ignored) the gate's own
    //     (x,z) onto every string's per-qubit (x,z) -- an XOR on one bit.
    //   - H, CX, CZ are Clifford (not Pauli) elements: applying them means
    //     *conjugation*, P -> U P U^ (the standard stabilizer-tableau
    //     update rule), since they have no (x,z) of their own to compose.
    PauliSetBDD apply_X(int q) const;
    PauliSetBDD apply_Z(int q) const;
    PauliSetBDD apply_H(int q) const;
    PauliSetBDD apply_CX(int control, int target) const;
    PauliSetBDD apply_CY(int control, int target) const;
    PauliSetBDD apply_CZ(int control, int target) const;

    // --- multiply by a whole Pauli subgroup --------------------------------
    // Composes (group-multiplies, phase ignored) *every* Pauli supported on
    // `qubits` onto every string in the set, and unions the results:
    //     { P * G : P in *this, G supported on `qubits` }
    // The identity is among those G, so *this is always a subset of the result.
    //
    // Those 4^|qubits| operators are not an arbitrary collection -- as GF(2)
    // vectors they are exactly the subspace spanned by the x/z coordinates of
    // `qubits`. Multiplying a set by a whole subspace is the union of its
    // cosets, whose characteristic function is precisely the original one with
    // those coordinates existentially quantified away. So this is one
    // bdd_exist, not 4^|qubits| compositions and unions; equivalently, it
    // *forgets* what the set said about `qubits`. Cost is independent of
    // 4^|qubits| -- the subset size only changes how many variables are
    // quantified.
    PauliSetBDD multiply_by_all_paulis_on(const std::vector<int> &qubits) const;
    PauliSetBDD multiply_by_all_paulis_on(int a, int b) const;  // the 16 two-qubit Paulis

    // Discard whatever these qubits carried and pin them back to identity --
    // the Pauli-frame effect of a physical reset, and the way an ancilla is
    // handed on to the next round once its outcome has been recorded.
    //
    // This is the forget above followed by an intersection with "these qubits
    // are I", so unlike a bare forget the set does not blow up by 4^|qubits|.
    // It is a genuine projection: two sets differing only on `qubits` collapse
    // together, which is right, since a reset erases exactly that difference.
    PauliSetBDD reset_qubits(const std::vector<int> &qubits) const;

    bool operator==(const PauliSetBDD &rhs) const;
    bool operator!=(const PauliSetBDD &rhs) const;

    // --- queries ------------------------------------------------------
    bool   contains(const std::vector<Pauli> &paulis) const;
    bool   contains(const std::string &paulis) const;
    bool   is_empty() const;
    double size() const;        // number of Pauli strings in the set
    int    node_count() const;  // number of BDD nodes (diagram size)

    // --- enumeration ------------------------------------------------------
    // Visit every Pauli string in the set, one at a time, in the current
    // variable order. Return false from fn to stop early.
    //
    // BuDDy has no built-in for this: bdd_allsat reports *cubes* carrying
    // don't-cares (a single callback can stand for 2^k strings) and
    // bdd_printset prints that same compressed form. This expands the
    // don't-cares so each callback is one concrete Pauli string.
    void for_each(const std::function<bool(const std::string &)> &fn) const;

    // Collect every Pauli string. Throws std::length_error if the set holds
    // more than max_count of them -- these sets run to 4^n, so an accidental
    // full enumeration is a real hazard. Check size() first.
    std::vector<std::string> to_strings(std::size_t max_count = 4096) const;

    // --- debugging ------------------------------------------------------
    void print_raw() const;   // dumps via bdd_printset (numeric variable indices)
    bdd  raw() const { return f_; }

    static Pauli char_to_pauli(char c);
    static char  pauli_to_char(Pauli p);

private:
    bdd f_;
    static int  n_;
    static bool initialized_;
    static void check_init();
};

} // namespace pbdd
