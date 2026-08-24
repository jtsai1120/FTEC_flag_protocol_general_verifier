OPENQASM 3.0;
include "stdgates.inc";

// [[19,1,5]] CSS code: parallel [2,2,2,1,1,1]^T flag-sharing SE.
// Source for the Z block: Liou & Lai, arXiv:2407.00607, Appendix C, Fig. 22.
// Paper convention:
//   measurement ancilla: |+>, measured in X basis;
//   flag ancilla:        |0>, measured in Z basis;
//   Z-data coupling: CZ(measurement, data);
//   X-data coupling: CNOT(measurement -> data);
//   flag coupling:  CNOT(measurement -> flag).
//
// Fig. 22 uses 9 measurement qubits and 9 physical flag qubits,
// 66 two-qubit gates, and two-qubit dependency depth 10 for one Pauli type.
// data[0] = physical q1, ..., data[18] = physical q19.

// This full file contains:
//   1) the exact Fig.22 Z-type block;
//   2) the CSS X-type counterpart using the same interaction/flag-sharing topology.
// The paper draws only the Z-type circuit in Fig.22; the X block below is the
// corresponding CSS construction used for a complete 18-bit syndrome round.
//
// IMPORTANT generator convention used here (same as the user's prior FSE_f.qasm):
// g10..g18 use the same supports as g1..g9 with X replacing Z, in particular
// g18 = X5 X7 X8 X11 X12 X13.

qubit[19] data;
qubit[9] syn;
qubit[9] flag;

bit[18] m;
bit[18] f;

// ============================================================
// Z block: g1..g9 -- exact transcription of Fig.22
// ============================================================
// Prepare 9 measurement ancillas in |+> and 9 flags in |0>.
reset syn[0];
h syn[0];
reset syn[1];
h syn[1];
reset syn[2];
h syn[2];
reset syn[3];
h syn[3];
reset syn[4];
h syn[4];
reset syn[5];
h syn[5];
reset syn[6];
h syn[6];
reset syn[7];
h syn[7];
reset syn[8];
h syn[8];
reset flag[0];
reset flag[1];
reset flag[2];
reset flag[3];
reset flag[4];
reset flag[5];
reset flag[6];
reset flag[7];
reset flag[8];

cz syn[3], data[0];  // Fig.22 gate 1: q1 - m4
cz syn[7], data[7];  // Fig.22 gate 2: q8 - m8
cz syn[8], data[4];  // Fig.22 gate 3: q5 - m9
cz syn[0], data[0];  // Fig.22 gate 4: q1 - m1
cx syn[3], flag[0];  // Fig.22 gate 5: m4 -> f1
cz syn[4], data[8];  // Fig.22 gate 6: q9 - m5
cx syn[7], flag[3];  // Fig.22 gate 7: m8 -> f4
cx syn[8], flag[4];  // Fig.22 gate 8: m9 -> f5
cx syn[0], flag[0];  // Fig.22 gate 9: m1 -> f1
cz syn[2], data[11];  // Fig.22 gate 10: q12 - m3
cz syn[3], data[1];  // Fig.22 gate 11: q2 - m4
cx syn[4], flag[6];  // Fig.22 gate 12: m5 -> f7
cz syn[7], data[8];  // Fig.22 gate 13: q9 - m8
cz syn[8], data[6];  // Fig.22 gate 14: q7 - m9
cz syn[0], data[1];  // Fig.22 gate 15: q2 - m1
cx syn[2], flag[4];  // Fig.22 gate 16: m3 -> f5
cx syn[3], flag[1];  // Fig.22 gate 17: m4 -> f2
cz syn[4], data[5];  // Fig.22 gate 18: q6 - m5
cz syn[5], data[15];  // Fig.22 gate 19: q16 - m6
cz syn[6], data[9];  // Fig.22 gate 20: q10 - m7
cx syn[7], flag[7];  // Fig.22 gate 21: m8 -> f8
cx syn[8], flag[5];  // Fig.22 gate 22: m9 -> f6
cz syn[0], data[2];  // Fig.22 gate 23: q3 - m1
cz syn[1], data[0];  // Fig.22 gate 24: q1 - m2
cz syn[2], data[12];  // Fig.22 gate 25: q13 - m3
cz syn[3], data[4];  // Fig.22 gate 26: q5 - m4
cz syn[4], data[15];  // Fig.22 gate 27: q16 - m5
cx syn[5], flag[6];  // Fig.22 gate 28: m6 -> f7
cz syn[7], data[10];  // Fig.22 gate 29: q11 - m8
cz syn[8], data[7];  // Fig.22 gate 30: q8 - m9
cx syn[0], flag[0];  // Fig.22 gate 31: m1 -> f1
cx syn[1], flag[2];  // Fig.22 gate 32: m2 -> f3
cz syn[2], data[13];  // Fig.22 gate 33: q14 - m3
cz syn[3], data[5];  // Fig.22 gate 34: q6 - m4
cx syn[4], flag[6];  // Fig.22 gate 35: m5 -> f7
cz syn[5], data[18];  // Fig.22 gate 36: q19 - m6
cx syn[6], flag[8];  // Fig.22 gate 37: m7 -> f9
cz syn[7], data[9];  // Fig.22 gate 38: q10 - m8
cz syn[8], data[10];  // Fig.22 gate 39: q11 - m9
cz syn[0], data[3];  // Fig.22 gate 40: q4 - m1
cz syn[1], data[2];  // Fig.22 gate 41: q3 - m2
cx syn[2], flag[4];  // Fig.22 gate 42: m3 -> f5
cx syn[3], flag[0];  // Fig.22 gate 43: m4 -> f1
cz syn[4], data[18];  // Fig.22 gate 44: q19 - m5
cz syn[5], data[16];  // Fig.22 gate 45: q17 - m6
cz syn[6], data[11];  // Fig.22 gate 46: q12 - m7
cx syn[7], flag[3];  // Fig.22 gate 47: m8 -> f4
cx syn[8], flag[4];  // Fig.22 gate 48: m9 -> f5
cz syn[1], data[4];  // Fig.22 gate 49: q5 - m2
cz syn[2], data[14];  // Fig.22 gate 50: q15 - m3
cz syn[3], data[7];  // Fig.22 gate 51: q8 - m4
cx syn[5], flag[6];  // Fig.22 gate 52: m6 -> f7
cz syn[6], data[10];  // Fig.22 gate 53: q11 - m7
cz syn[7], data[15];  // Fig.22 gate 54: q16 - m8
cz syn[8], data[11];  // Fig.22 gate 55: q12 - m9
cx syn[1], flag[2];  // Fig.22 gate 56: m2 -> f3
cx syn[3], flag[1];  // Fig.22 gate 57: m4 -> f2
cx syn[6], flag[8];  // Fig.22 gate 58: m7 -> f9
cx syn[7], flag[7];  // Fig.22 gate 59: m8 -> f8
cx syn[8], flag[5];  // Fig.22 gate 60: m9 -> f6
cz syn[1], data[6];  // Fig.22 gate 61: q7 - m2
cz syn[3], data[8];  // Fig.22 gate 62: q9 - m4
cz syn[5], data[17];  // Fig.22 gate 63: q18 - m6
cz syn[6], data[14];  // Fig.22 gate 64: q15 - m7
cz syn[7], data[16];  // Fig.22 gate 65: q17 - m8
cz syn[8], data[12];  // Fig.22 gate 66: q13 - m9

// Measurement ancillas in X basis.
h syn[0];
m[0] = measure syn[0];
h syn[1];
m[1] = measure syn[1];
h syn[2];
m[2] = measure syn[2];
h syn[3];
m[3] = measure syn[3];
h syn[4];
m[4] = measure syn[4];
h syn[5];
m[5] = measure syn[5];
h syn[6];
m[6] = measure syn[6];
h syn[7];
m[7] = measure syn[7];
h syn[8];
m[8] = measure syn[8];

// Flag ancillas in Z basis.
f[0] = measure flag[0];
f[1] = measure flag[1];
f[2] = measure flag[2];
f[3] = measure flag[3];
f[4] = measure flag[4];
f[5] = measure flag[5];
f[6] = measure flag[6];
f[7] = measure flag[7];
f[8] = measure flag[8];

barrier data, syn, flag;

// ============================================================
// X block: g10..g18 -- CSS counterpart of the Fig.22 topology
// ============================================================
// Prepare 9 measurement ancillas in |+> and 9 flags in |0>.
reset syn[0];
h syn[0];
reset syn[1];
h syn[1];
reset syn[2];
h syn[2];
reset syn[3];
h syn[3];
reset syn[4];
h syn[4];
reset syn[5];
h syn[5];
reset syn[6];
h syn[6];
reset syn[7];
h syn[7];
reset syn[8];
h syn[8];
reset flag[0];
reset flag[1];
reset flag[2];
reset flag[3];
reset flag[4];
reset flag[5];
reset flag[6];
reset flag[7];
reset flag[8];

cx syn[3], data[0];  // X counterpart of Fig.22 gate 1: q1 - m4
cx syn[7], data[7];  // X counterpart of Fig.22 gate 2: q8 - m8
cx syn[8], data[4];  // X counterpart of Fig.22 gate 3: q5 - m9
cx syn[0], data[0];  // X counterpart of Fig.22 gate 4: q1 - m1
cx syn[3], flag[0];  // Fig.22 gate 5: m4 -> f1
cx syn[4], data[8];  // X counterpart of Fig.22 gate 6: q9 - m5
cx syn[7], flag[3];  // Fig.22 gate 7: m8 -> f4
cx syn[8], flag[4];  // Fig.22 gate 8: m9 -> f5
cx syn[0], flag[0];  // Fig.22 gate 9: m1 -> f1
cx syn[2], data[11];  // X counterpart of Fig.22 gate 10: q12 - m3
cx syn[3], data[1];  // X counterpart of Fig.22 gate 11: q2 - m4
cx syn[4], flag[6];  // Fig.22 gate 12: m5 -> f7
cx syn[7], data[8];  // X counterpart of Fig.22 gate 13: q9 - m8
cx syn[8], data[6];  // X counterpart of Fig.22 gate 14: q7 - m9
cx syn[0], data[1];  // X counterpart of Fig.22 gate 15: q2 - m1
cx syn[2], flag[4];  // Fig.22 gate 16: m3 -> f5
cx syn[3], flag[1];  // Fig.22 gate 17: m4 -> f2
cx syn[4], data[5];  // X counterpart of Fig.22 gate 18: q6 - m5
cx syn[5], data[15];  // X counterpart of Fig.22 gate 19: q16 - m6
cx syn[6], data[9];  // X counterpart of Fig.22 gate 20: q10 - m7
cx syn[7], flag[7];  // Fig.22 gate 21: m8 -> f8
cx syn[8], flag[5];  // Fig.22 gate 22: m9 -> f6
cx syn[0], data[2];  // X counterpart of Fig.22 gate 23: q3 - m1
cx syn[1], data[0];  // X counterpart of Fig.22 gate 24: q1 - m2
cx syn[2], data[12];  // X counterpart of Fig.22 gate 25: q13 - m3
cx syn[3], data[4];  // X counterpart of Fig.22 gate 26: q5 - m4
cx syn[4], data[15];  // X counterpart of Fig.22 gate 27: q16 - m5
cx syn[5], flag[6];  // Fig.22 gate 28: m6 -> f7
cx syn[7], data[10];  // X counterpart of Fig.22 gate 29: q11 - m8
cx syn[8], data[7];  // X counterpart of Fig.22 gate 30: q8 - m9
cx syn[0], flag[0];  // Fig.22 gate 31: m1 -> f1
cx syn[1], flag[2];  // Fig.22 gate 32: m2 -> f3
cx syn[2], data[13];  // X counterpart of Fig.22 gate 33: q14 - m3
cx syn[3], data[5];  // X counterpart of Fig.22 gate 34: q6 - m4
cx syn[4], flag[6];  // Fig.22 gate 35: m5 -> f7
cx syn[5], data[18];  // X counterpart of Fig.22 gate 36: q19 - m6
cx syn[6], flag[8];  // Fig.22 gate 37: m7 -> f9
cx syn[7], data[9];  // X counterpart of Fig.22 gate 38: q10 - m8
cx syn[8], data[10];  // X counterpart of Fig.22 gate 39: q11 - m9
cx syn[0], data[3];  // X counterpart of Fig.22 gate 40: q4 - m1
cx syn[1], data[2];  // X counterpart of Fig.22 gate 41: q3 - m2
cx syn[2], flag[4];  // Fig.22 gate 42: m3 -> f5
cx syn[3], flag[0];  // Fig.22 gate 43: m4 -> f1
cx syn[4], data[18];  // X counterpart of Fig.22 gate 44: q19 - m5
cx syn[5], data[16];  // X counterpart of Fig.22 gate 45: q17 - m6
cx syn[6], data[11];  // X counterpart of Fig.22 gate 46: q12 - m7
cx syn[7], flag[3];  // Fig.22 gate 47: m8 -> f4
cx syn[8], flag[4];  // Fig.22 gate 48: m9 -> f5
cx syn[1], data[4];  // X counterpart of Fig.22 gate 49: q5 - m2
cx syn[2], data[14];  // X counterpart of Fig.22 gate 50: q15 - m3
cx syn[3], data[7];  // X counterpart of Fig.22 gate 51: q8 - m4
cx syn[5], flag[6];  // Fig.22 gate 52: m6 -> f7
cx syn[6], data[10];  // X counterpart of Fig.22 gate 53: q11 - m7
cx syn[7], data[15];  // X counterpart of Fig.22 gate 54: q16 - m8
cx syn[8], data[11];  // X counterpart of Fig.22 gate 55: q12 - m9
cx syn[1], flag[2];  // Fig.22 gate 56: m2 -> f3
cx syn[3], flag[1];  // Fig.22 gate 57: m4 -> f2
cx syn[6], flag[8];  // Fig.22 gate 58: m7 -> f9
cx syn[7], flag[7];  // Fig.22 gate 59: m8 -> f8
cx syn[8], flag[5];  // Fig.22 gate 60: m9 -> f6
cx syn[1], data[6];  // X counterpart of Fig.22 gate 61: q7 - m2
cx syn[3], data[8];  // X counterpart of Fig.22 gate 62: q9 - m4
cx syn[5], data[17];  // X counterpart of Fig.22 gate 63: q18 - m6
cx syn[6], data[14];  // X counterpart of Fig.22 gate 64: q15 - m7
cx syn[7], data[16];  // X counterpart of Fig.22 gate 65: q17 - m8
cx syn[8], data[12];  // X counterpart of Fig.22 gate 66: q13 - m9

// Measurement ancillas in X basis.
h syn[0];
m[9] = measure syn[0];
h syn[1];
m[10] = measure syn[1];
h syn[2];
m[11] = measure syn[2];
h syn[3];
m[12] = measure syn[3];
h syn[4];
m[13] = measure syn[4];
h syn[5];
m[14] = measure syn[5];
h syn[6];
m[15] = measure syn[6];
h syn[7];
m[16] = measure syn[7];
h syn[8];
m[17] = measure syn[8];

// Flag ancillas in Z basis.
f[9] = measure flag[0];
f[10] = measure flag[1];
f[11] = measure flag[2];
f[12] = measure flag[3];
f[13] = measure flag[4];
f[14] = measure flag[5];
f[15] = measure flag[6];
f[16] = measure flag[7];
f[17] = measure flag[8];
