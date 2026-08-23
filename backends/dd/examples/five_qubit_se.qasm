// A stand-in syndrome-extraction round for the [[5,1,3]] code.
// qd is the 5-qubit data block, qm a single ancilla.

OPENQASM 3.0;
include "stdgates.inc";

qubit[5] qd;
qubit[1] qm;

h qm[0];
cx qd[0], qm[0];
cz qd[1], qm[0];
cz qd[2], qm[0];
cx qd[3], qm[0];
h qm[0];
