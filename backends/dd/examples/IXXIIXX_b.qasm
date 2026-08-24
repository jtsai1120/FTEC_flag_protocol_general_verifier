OPENQASM 3.0;
include "stdgates.inc";

// Bare syndrome extraction for stabilizer IXXIIXX.
// qd[0..6] = data qubits, qm[0] = syndrome ancilla.
qubit[7] qd;
qubit[1] qm;

h qd[1];
cx qd[1], qm[0];
h qd[1];

h qd[2];
cx qd[2], qm[0];
h qd[2];

h qd[5];
cx qd[5], qm[0];
h qd[5];

h qd[6];
cx qd[6], qm[0];
h qd[6];
