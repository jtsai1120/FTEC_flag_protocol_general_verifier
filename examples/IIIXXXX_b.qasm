OPENQASM 3.0;
include "stdgates.inc";

// Bare syndrome extraction for stabilizer IIIXXXX.
// qd[0..6] = data qubits, qm[0] = syndrome ancilla.
qubit[7] qd;
qubit[1] qm;

h qd[3];
cx qd[3], qm[0];
h qd[3];

h qd[4];
cx qd[4], qm[0];
h qd[4];

h qd[5];
cx qd[5], qm[0];
h qd[5];

h qd[6];
cx qd[6], qm[0];
h qd[6];
