OPENQASM 3.0;
include "stdgates.inc";

// Combined serial unflagged syndrome extraction for the [[19,1,5]] CSS code.
// Generator order follows arXiv:2407.00607, g1..g18.
// data[0] = q1, ..., data[18] = q19.
// syn is the measurement ancilla, reused for every generator.
// m[i] stores the syndrome bit for g(i+1).

qubit[19] data;
qubit syn;

bit[18] m;


// ------------------------------------------------------------
// Unflagged SE for g1 = Z1Z2Z3Z4
// ------------------------------------------------------------
reset syn;

cx data[0], syn;
cx data[1], syn;
cx data[2], syn;
cx data[3], syn;

m[0] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g2 = Z1Z3Z5Z7
// ------------------------------------------------------------
reset syn;

cx data[0], syn;
cx data[2], syn;
cx data[4], syn;
cx data[6], syn;

m[1] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g3 = Z12Z13Z14Z15
// ------------------------------------------------------------
reset syn;

cx data[11], syn;
cx data[12], syn;
cx data[13], syn;
cx data[14], syn;

m[2] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g4 = Z1Z2Z5Z6Z8Z9
// ------------------------------------------------------------
reset syn;

cx data[0], syn;
cx data[1], syn;
cx data[4], syn;
cx data[5], syn;
cx data[7], syn;
cx data[8], syn;

m[3] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g5 = Z6Z9Z16Z19
// ------------------------------------------------------------
reset syn;

cx data[5], syn;
cx data[8], syn;
cx data[15], syn;
cx data[18], syn;

m[4] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g6 = Z16Z17Z18Z19
// ------------------------------------------------------------
reset syn;

cx data[15], syn;
cx data[16], syn;
cx data[17], syn;
cx data[18], syn;

m[5] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g7 = Z10Z11Z12Z15
// ------------------------------------------------------------
reset syn;

cx data[9], syn;
cx data[10], syn;
cx data[11], syn;
cx data[14], syn;

m[6] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g8 = Z8Z9Z10Z11Z16Z17
// ------------------------------------------------------------
reset syn;

cx data[7], syn;
cx data[8], syn;
cx data[9], syn;
cx data[10], syn;
cx data[15], syn;
cx data[16], syn;

m[7] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g9 = Z5Z7Z8Z11Z12Z13
// ------------------------------------------------------------
reset syn;

cx data[4], syn;
cx data[6], syn;
cx data[7], syn;
cx data[10], syn;
cx data[11], syn;
cx data[12], syn;

m[8] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g10 = X1X2X3X4
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[0];
h data[1];
h data[2];
h data[3];

cx data[0], syn;
cx data[1], syn;
cx data[2], syn;
cx data[3], syn;

// Restore data basis.
h data[0];
h data[1];
h data[2];
h data[3];

m[9] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g11 = X1X3X5X7
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[0];
h data[2];
h data[4];
h data[6];

cx data[0], syn;
cx data[2], syn;
cx data[4], syn;
cx data[6], syn;

// Restore data basis.
h data[0];
h data[2];
h data[4];
h data[6];

m[10] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g12 = X12X13X14X15
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[11];
h data[12];
h data[13];
h data[14];

cx data[11], syn;
cx data[12], syn;
cx data[13], syn;
cx data[14], syn;

// Restore data basis.
h data[11];
h data[12];
h data[13];
h data[14];

m[11] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g13 = X1X2X5X6X8X9
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[0];
h data[1];
h data[4];
h data[5];
h data[7];
h data[8];

cx data[0], syn;
cx data[1], syn;
cx data[4], syn;
cx data[5], syn;
cx data[7], syn;
cx data[8], syn;

// Restore data basis.
h data[0];
h data[1];
h data[4];
h data[5];
h data[7];
h data[8];

m[12] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g14 = X6X9X16X19
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[5];
h data[8];
h data[15];
h data[18];

cx data[5], syn;
cx data[8], syn;
cx data[15], syn;
cx data[18], syn;

// Restore data basis.
h data[5];
h data[8];
h data[15];
h data[18];

m[13] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g15 = X16X17X18X19
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[15];
h data[16];
h data[17];
h data[18];

cx data[15], syn;
cx data[16], syn;
cx data[17], syn;
cx data[18], syn;

// Restore data basis.
h data[15];
h data[16];
h data[17];
h data[18];

m[14] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g16 = X10X11X12X15
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[9];
h data[10];
h data[11];
h data[14];

cx data[9], syn;
cx data[10], syn;
cx data[11], syn;
cx data[14], syn;

// Restore data basis.
h data[9];
h data[10];
h data[11];
h data[14];

m[15] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g17 = X8X9X10X11X16X17
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[7];
h data[8];
h data[9];
h data[10];
h data[15];
h data[16];

cx data[7], syn;
cx data[8], syn;
cx data[9], syn;
cx data[10], syn;
cx data[15], syn;
cx data[16], syn;

// Restore data basis.
h data[7];
h data[8];
h data[9];
h data[10];
h data[15];
h data[16];

m[16] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for g18 = X5X7X8X11X12X13
// ------------------------------------------------------------
reset syn;

// Convert X-parity extraction to Z-style CNOT parity extraction.
h data[4];
h data[6];
h data[7];
h data[10];
h data[11];
h data[12];

cx data[4], syn;
cx data[6], syn;
cx data[7], syn;
cx data[10], syn;
cx data[11], syn;
cx data[12], syn;

// Restore data basis.
h data[4];
h data[6];
h data[7];
h data[10];
h data[11];
h data[12];

m[17] = measure syn;
barrier data, syn;
