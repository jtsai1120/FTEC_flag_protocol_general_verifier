#pragma once

#include "ftec/backend.hpp"

#include <memory>

namespace ftec {

// Pauli sets as binary decision diagrams.
//
// A state is the family of sets indexed by fault count: by_t[t] holds every
// error configuration reachable with exactly t faults. Running a circuit walks
// its instructions in order --
//
//   gate         transform every level
//   two-qubit    also inject: level t spawns into level t+1 by multiplying in
//                all 16 Paulis on the two qubits the gate touched
//   measure      split every level on the measured qubit's x component, which
//                is what a Z-basis measurement sees, and append the bit to the
//                classical register named in the circuit
//   reset        forget the qubit and pin it back to identity
//
// -- so measurement is an event in the middle of a circuit rather than
// something that happens at the end. That is not a refinement: FSE_b.qasm
// reuses one ancilla four times with a reset between measurements, and an
// end-of-circuit model has nowhere to put the first three outcomes.
//
// BuDDy is a process-wide singleton, so at most one of these may exist at a
// time and it owns the session for its lifetime.
std::unique_ptr<Backend> make_dd_backend();

} // namespace ftec
