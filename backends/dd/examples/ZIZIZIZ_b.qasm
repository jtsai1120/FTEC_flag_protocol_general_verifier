OPENQASM 3.0;
include "stdgates.inc";

// Bare syndrome extraction for stabilizer ZIZIZIZ.
// qd[0..6] = data qubits, qm[0] = syndrome ancilla.
qubit[7] qd;
qubit[1] qm;

cx qd[0], qm[0];
cx qd[2], qm[0];
cx qd[4], qm[0];
cx qd[6], qm[0];
