// A small syndrome-extraction-shaped circuit in the supported OpenQASM 3 subset.
//
// Exactly two registers are required: qd (data) and qm (measurement).
// Only x, z, h, cx, cy, cz on explicitly indexed qubits are allowed; barrier
// is ignored; measure and reset are rejected for now.
//
// Fault locations are the two-qubit gates only (cx / cy / cz) -- this circuit
// has 6 of them, so t can reach 6.
//
// Measurement is in the Z basis, so the syndrome bit for qm[j] is set exactly
// when the Pauli that ends up on it anticommutes with Z (X or Y). To measure
// in the X basis instead, put an h on that qubit before the end.

OPENQASM 3.0;
include "stdgates.inc";

qubit[4] qd;
qubit[2] qm;

// Two Z-type checks: copy X information from the data qubits onto the ancillas.
h qm[1];

cx qd[0], qm[0];
cx qm[1], qm[0];
cx qd[1], qm[0];
cx qd[2], qm[0];
cx qm[1], qm[0];
cx qd[3], qm[0];

h qm[1];

