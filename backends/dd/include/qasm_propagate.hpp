#pragma once

#include "pauli_bdd.hpp"

#include <string>
#include <vector>

namespace pbdd {

// One syndrome branch: the subset of the reachable Pauli set at fault count t
// whose measurement qubits produce the syndrome recorded in mr.
//
// Across a chain of circuits mr grows one round at a time, rounds separated by
// PauliFlow::kRoundSeparator, e.g. "01|10" after two rounds of two ancillas.
// (t, mr) is the key: two paths reaching the same total fault count with the
// same record are indistinguishable and get unioned.
struct SyndromeBranch {
    int         t;    // number of faults so far
    std::string mr;   // measurement record; mr[j] is qm[j] within its round
    PauliSetBDD set;
};

// Propagates the all-identity Pauli through an OpenQASM 3 circuit, injecting
// every possible two-qubit fault at every two-qubit gate, keeps the reachable
// Pauli set for each fault count t = 0..tau, and finally splits each of those
// by the syndrome its measurement qubits carry.
//
// REGISTERS: the circuit must declare exactly two, named qd (data) and qm
// (measurement), both non-empty. They are flattened into one qubit index
// space, data first:
//     qd[i] -> global qubit i          (0 .. nd-1)
//     qm[j] -> global qubit nd + j     (nd .. nd+nm-1)
// So position i of a Pauli string is qd[i] while i < nd, and qm[i-nd] after.
// Two-qubit gates may span the two registers -- that is what makes syndrome
// extraction expressible.
//
// PER TIC (one gate, in file order, even when gates could run in parallel):
//   (A) apply the gate to the set held at every t;
//   (B) only at a two-qubit gate (cx/cy/cz), each t < tau spawns a new set by
//       multiplying in all 16 Paulis on that gate's two qubits, at t+1 --
//       the original set stays, representing "this location did not fault";
//   (C) each spawned set is unioned into whatever already sits at t+1.
// All spawns are computed from the post-(A), pre-(C) state, so a single tic
// adds at most one fault to any lineage. Single-qubit gates are assumed
// fault-free and therefore never spawn.
//
// Every gate conjugates the set: an error is defined against what the ideal
// circuit would have produced, and the ideal state moves under the gate too.
// For x and z that conjugation is the identity, since X P X^ and Z P Z^ differ
// from P only by a phase this representation does not track.
//
// SYNDROME SPLIT (after the last tic): measurement is in the Z basis, so the
// outcome for qm[j] flips exactly when the Pauli sitting on it anticommutes
// with Z -- that is, when its x component is set (X and Y flip; I and Z do
// not). Each by_t()[t] is therefore split on the x variables of the qm qubits
// into at most 2^nm branches, empty ones dropped. The branches at one t are
// pairwise disjoint and their union is by_t()[t]. To measure in the X basis,
// put an h before the measurement in the circuit.
//
// OWNERSHIP: BuDDy is a process-wide singleton, so this object owns the whole
// BDD session -- PauliSetBDD::done() runs in its destructor. Every PauliSetBDD
// it hands out is valid only while it is alive, and at most one of these may
// exist at a time. Let one go out of scope before calling propagate_qasm again.
class QasmPropagation {
public:
    QasmPropagation(QasmPropagation &&other) noexcept;
    QasmPropagation &operator=(QasmPropagation &&other) noexcept;
    QasmPropagation(const QasmPropagation &) = delete;
    QasmPropagation &operator=(const QasmPropagation &) = delete;
    ~QasmPropagation();

    int n_qubits() const { return n_data_ + n_measure_; }
    int n_data() const { return n_data_; }
    int n_measure() const { return n_measure_; }
    int tau() const { return tau_; }
    int n_tics() const { return n_tics_; }              // = number of gates
    int n_fault_locations() const { return n_faults_; } // = number of 2-qubit gates

    // Register index -> global qubit index, for building query strings.
    int data_qubit(int i) const;
    int measure_qubit(int j) const;

    // Reachable set at exactly t faults, before the syndrome split.
    const PauliSetBDD &at(int t) const;
    const std::vector<PauliSetBDD> &by_t() const { return by_t_; }

    // Non-empty syndrome branches, ordered by t and then by mr.
    const std::vector<SyndromeBranch> &branches() const { return branches_; }

private:
    friend QasmPropagation propagate_qasm(const std::string &path, int tau,
                                          bool reset_measure);

    QasmPropagation() = default;
    void release() noexcept;

    int  n_data_    = 0;
    int  n_measure_ = 0;
    int  tau_       = 0;
    int  n_tics_    = 0;
    int  n_faults_  = 0;
    bool owns_session_ = false;
    std::vector<PauliSetBDD>    by_t_;
    std::vector<SyndromeBranch> branches_;
};

// Reads and validates `path`, then runs the propagation described above.
// Throws std::runtime_error on any unsupported construct: a gate other than
// x/z/h/cx/cy/cz (measure and reset included), a register-wide gate such as
// "h qd;", a register not named qd or qm, a missing or empty register, an
// out-of-range index, or a missing OPENQASM 3 header. `barrier` is ignored;
// `include "stdgates.inc"` is accepted and ignored.
//
// reset_measure pins every measurement qubit back to identity in the returned
// branches, which is what a physical reset does once the outcome is recorded.
// Pass false to inspect what the ancillas actually carried; note that only
// then do the branches at one t partition at(t).
QasmPropagation propagate_qasm(const std::string &path, int tau,
                               bool reset_measure = true);

// A chain of circuits sharing one fault budget and one BuDDy session -- the
// "path" through a sequence of syndrome-extraction rounds.
//
// Each run() propagates the branches carried so far through another circuit,
// splits them on that circuit's syndrome, appends the outcome to every mr, and
// (by default) resets the measurement qubits so the next circuit starts with
// fresh ancillas. Only the data register carries information across rounds.
//
// Branches sharing an mr are propagated together as one t-indexed state, so
// the number of independent propagations is the number of distinct records,
// not the number of branches -- and paths that land on the same (t, mr) merge
// for free, which is required: reaching total fault count t with record mr is
// the same observable outcome however the faults were distributed over rounds.
//
// The data register must have the same width in every circuit. The measurement
// register may differ; the qubit space grows to fit the widest one seen so far
// and unused ancilla slots stay at identity.
//
// OWNERSHIP: as with QasmPropagation this object owns the whole BuDDy session,
// so at most one PauliFlow or QasmPropagation may exist at a time, and every
// PauliSetBDD it hands out dies with it.
class PauliFlow {
public:
    static constexpr char kRoundSeparator = '|';

    // tau is the budget for the whole chain, not per circuit.
    explicit PauliFlow(int tau);
    PauliFlow(PauliFlow &&other) noexcept;
    PauliFlow &operator=(PauliFlow &&other) noexcept;
    PauliFlow(const PauliFlow &) = delete;
    PauliFlow &operator=(const PauliFlow &) = delete;
    ~PauliFlow();

    void run(const std::string &qasm_path, bool reset_measure = true);

    int tau() const { return tau_; }
    int n_rounds() const { return n_rounds_; }
    int n_data() const { return n_data_; }
    int n_measure() const { return n_measure_; }  // widest round so far
    int n_qubits() const { return n_data_ + n_measure_; }

    // Empty until the first run(); ordered by (t, mr) after each one.
    const std::vector<SyndromeBranch> &branches() const { return branches_; }

private:
    void release() noexcept;

    int  tau_       = 0;
    int  n_rounds_  = 0;
    int  n_data_    = 0;
    int  n_measure_ = 0;
    bool owns_session_ = false;
    std::vector<SyndromeBranch> branches_;
};

} // namespace pbdd
