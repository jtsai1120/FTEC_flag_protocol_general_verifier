OPENQASM 3.0;
include "stdgates.inc";

// Steane [[7,1,3]] syndrome-extraction circuit.
// Stabilizer: IZZXXYY
// qd[0..6] = data qubits, qm[0] = syndrome ancilla, qm[1] = flag ancilla.
// This version uses CY directly on the Y support.
qubit[7] qd;
qubit[2] qm;

h qm[1];

// Z on qd[1]
cx qd[1], qm[0];

// First flag coupling
cx qm[1], qm[0];

// Z on qd[2]
cx qd[2], qm[0];

// X on qd[3]
h qd[3];
cx qd[3], qm[0];
h qd[3];

// X on qd[4]
h qd[4];
cx qd[4], qm[0];
h qd[4];

// Y on qd[5]
cy qd[5], qm[0];

// Second flag coupling
cx qm[1], qm[0];

// Y on qd[6]
cy qd[6], qm[0];

h qm[1];
