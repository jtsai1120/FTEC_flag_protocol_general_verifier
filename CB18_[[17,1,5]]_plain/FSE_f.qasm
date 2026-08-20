OPENQASM 3.0;
include "stdgates.inc";

// Combined serial flagged syndrome extraction for the [[17,1,5]] 2D color code.
// Source: Chamberland & Beverland, arXiv:1708.02246, Table 7.
// Generator order follows Table 7.

// data[0] = q1, ..., data[16] = q17.
// syn is the syndrome/measurement ancilla, reused for every generator.
// flag[0..2] are reusable physical flag ancillas.
// m[i] stores the syndrome bit for generator gi.
// f is a compact classical flag record with no unused bits.
// For generator gi, its flag outcomes start at the offset shown in comments below.

qubit[17] data;
qubit syn;
qubit[3] flag;

bit[16] m;
bit[20] f;

// Compact flag-record layout:
// g0 = Z1Z2Z3Z4: f[0]
// g1 = X1X2X3X4: f[1]
// g2 = Z1Z3Z5Z6: f[2]
// g3 = X1X3X5X6: f[3]
// g4 = Z5Z6Z9Z10: f[4]
// g5 = X5X6X9X10: f[5]
// g6 = Z7Z8Z11Z12: f[6]
// g7 = X7X8X11X12: f[7]
// g8 = Z9Z10Z13Z14: f[8]
// g9 = X9X10X13X14: f[9]
// g10 = Z11Z12Z15Z16: f[10]
// g11 = X11X12X15X16: f[11]
// g12 = Z8Z12Z16Z17: f[12]
// g13 = X8X12X16X17: f[13]
// g14 = Z3Z4Z6Z7Z10Z11Z14Z15: f[14]..f[16]
// g15 = X3X4X6X7X10X11X14X15: f[17]..f[19]

// ------------------------------------------------------------
// Flagged SE for g0 = Z1Z2Z3Z4
// weight 4; uses 1 flag qubit(s); records to f[0]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[0], syn;
cx flag[0], syn;
cx data[1], syn;
cx data[2], syn;
cx flag[0], syn;
cx data[3], syn;

// Measure syndrome in Z basis.
m[0] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[0] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g1 = X1X2X3X4
// weight 4; uses 1 flag qubit(s); records to f[1]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[0];
h data[1];
h data[2];
h data[3];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[0], syn;
cx flag[0], syn;
cx data[1], syn;
cx data[2], syn;
cx flag[0], syn;
cx data[3], syn;

// Restore data basis.
h data[0];
h data[1];
h data[2];
h data[3];

// Measure syndrome in Z basis.
m[1] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[1] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g2 = Z1Z3Z5Z6
// weight 4; uses 1 flag qubit(s); records to f[2]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[0], syn;
cx flag[0], syn;
cx data[2], syn;
cx data[4], syn;
cx flag[0], syn;
cx data[5], syn;

// Measure syndrome in Z basis.
m[2] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[2] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g3 = X1X3X5X6
// weight 4; uses 1 flag qubit(s); records to f[3]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[0];
h data[2];
h data[4];
h data[5];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[0], syn;
cx flag[0], syn;
cx data[2], syn;
cx data[4], syn;
cx flag[0], syn;
cx data[5], syn;

// Restore data basis.
h data[0];
h data[2];
h data[4];
h data[5];

// Measure syndrome in Z basis.
m[3] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[3] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g4 = Z5Z6Z9Z10
// weight 4; uses 1 flag qubit(s); records to f[4]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[4], syn;
cx flag[0], syn;
cx data[5], syn;
cx data[8], syn;
cx flag[0], syn;
cx data[9], syn;

// Measure syndrome in Z basis.
m[4] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[4] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g5 = X5X6X9X10
// weight 4; uses 1 flag qubit(s); records to f[5]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[4];
h data[5];
h data[8];
h data[9];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[4], syn;
cx flag[0], syn;
cx data[5], syn;
cx data[8], syn;
cx flag[0], syn;
cx data[9], syn;

// Restore data basis.
h data[4];
h data[5];
h data[8];
h data[9];

// Measure syndrome in Z basis.
m[5] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[5] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g6 = Z7Z8Z11Z12
// weight 4; uses 1 flag qubit(s); records to f[6]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[6], syn;
cx flag[0], syn;
cx data[7], syn;
cx data[10], syn;
cx flag[0], syn;
cx data[11], syn;

// Measure syndrome in Z basis.
m[6] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[6] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g7 = X7X8X11X12
// weight 4; uses 1 flag qubit(s); records to f[7]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[6];
h data[7];
h data[10];
h data[11];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[6], syn;
cx flag[0], syn;
cx data[7], syn;
cx data[10], syn;
cx flag[0], syn;
cx data[11], syn;

// Restore data basis.
h data[6];
h data[7];
h data[10];
h data[11];

// Measure syndrome in Z basis.
m[7] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[7] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g8 = Z9Z10Z13Z14
// weight 4; uses 1 flag qubit(s); records to f[8]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[8], syn;
cx flag[0], syn;
cx data[9], syn;
cx data[12], syn;
cx flag[0], syn;
cx data[13], syn;

// Measure syndrome in Z basis.
m[8] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[8] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g9 = X9X10X13X14
// weight 4; uses 1 flag qubit(s); records to f[9]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[8];
h data[9];
h data[12];
h data[13];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[8], syn;
cx flag[0], syn;
cx data[9], syn;
cx data[12], syn;
cx flag[0], syn;
cx data[13], syn;

// Restore data basis.
h data[8];
h data[9];
h data[12];
h data[13];

// Measure syndrome in Z basis.
m[9] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[9] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g10 = Z11Z12Z15Z16
// weight 4; uses 1 flag qubit(s); records to f[10]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[10], syn;
cx flag[0], syn;
cx data[11], syn;
cx data[14], syn;
cx flag[0], syn;
cx data[15], syn;

// Measure syndrome in Z basis.
m[10] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[10] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g11 = X11X12X15X16
// weight 4; uses 1 flag qubit(s); records to f[11]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[10];
h data[11];
h data[14];
h data[15];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[10], syn;
cx flag[0], syn;
cx data[11], syn;
cx data[14], syn;
cx flag[0], syn;
cx data[15], syn;

// Restore data basis.
h data[10];
h data[11];
h data[14];
h data[15];

// Measure syndrome in Z basis.
m[11] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[11] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g12 = Z8Z12Z16Z17
// weight 4; uses 1 flag qubit(s); records to f[12]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[7], syn;
cx flag[0], syn;
cx data[11], syn;
cx data[15], syn;
cx flag[0], syn;
cx data[16], syn;

// Measure syndrome in Z basis.
m[12] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[12] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g13 = X8X12X16X17
// weight 4; uses 1 flag qubit(s); records to f[13]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[7];
h data[11];
h data[15];
h data[16];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[7], syn;
cx flag[0], syn;
cx data[11], syn;
cx data[15], syn;
cx flag[0], syn;
cx data[16], syn;

// Restore data basis.
h data[7];
h data[11];
h data[15];
h data[16];

// Measure syndrome in Z basis.
m[13] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[13] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g14 = Z3Z4Z6Z7Z10Z11Z14Z15
// weight 8; uses 3 flag qubit(s); records to f[14]..f[16]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>
reset flag[1];
h flag[1];  // prepare |+>
reset flag[2];
h flag[2];  // prepare |+>

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[2], syn;
cx flag[0], syn;
cx data[3], syn;
cx flag[1], syn;
cx data[5], syn;
cx flag[2], syn;
cx data[6], syn;
cx data[9], syn;
cx flag[0], syn;
cx data[10], syn;
cx flag[1], syn;
cx data[13], syn;
cx flag[2], syn;
cx data[14], syn;

// Measure syndrome in Z basis.
m[14] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[14] = measure flag[0];
h flag[1];
f[15] = measure flag[1];
h flag[2];
f[16] = measure flag[2];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g15 = X3X4X6X7X10X11X14X15
// weight 8; uses 3 flag qubit(s); records to f[17]..f[19]
// ------------------------------------------------------------
reset syn;
reset flag[0];
h flag[0];  // prepare |+>
reset flag[1];
h flag[1];  // prepare |+>
reset flag[2];
h flag[2];  // prepare |+>

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[2];
h data[3];
h data[5];
h data[6];
h data[9];
h data[10];
h data[13];
h data[14];

// Interleaved data-to-syndrome and flag-to-syndrome CNOTs.
cx data[2], syn;
cx flag[0], syn;
cx data[3], syn;
cx flag[1], syn;
cx data[5], syn;
cx flag[2], syn;
cx data[6], syn;
cx data[9], syn;
cx flag[0], syn;
cx data[10], syn;
cx flag[1], syn;
cx data[13], syn;
cx flag[2], syn;
cx data[14], syn;

// Restore data basis.
h data[2];
h data[3];
h data[5];
h data[6];
h data[9];
h data[10];
h data[13];
h data[14];

// Measure syndrome in Z basis.
m[15] = measure syn;

// Measure used flag qubit(s) in X basis, compactly stored.
h flag[0];
f[17] = measure flag[0];
h flag[1];
f[18] = measure flag[1];
h flag[2];
f[19] = measure flag[2];

barrier data, syn, flag;
