OPENQASM 3.0;
include "stdgates.inc";

// Combined serial flagged extraction for the [[25,1,5]] rotated surface code.
// Source geometry: Chamberland--Beverland Figure 8.  Data qubits are numbered
// row-major on the 5x5 lattice.  Syndrome bits m[0..11] are Z checks and
// m[12..23] are X checks.  The reusable flag implements Figure 2(b).
qubit[25] data;
qubit syn;
qubit flag;
bit[24] m;
bit[16] f;

// g0 = Z1 Z6 (left boundary, weight 2; no flag required).
reset syn;
cx data[0],syn; cx data[5],syn;
m[0]=measure syn;
barrier data,syn,flag;

// g1 = Z2 Z3 Z7 Z8; flag f0.
reset syn; reset flag; h flag;
cx data[1],syn; cx flag,syn; cx data[2],syn;
cx data[6],syn; cx flag,syn; cx data[7],syn;
m[1]=measure syn; h flag; f[0]=measure flag;
barrier data,syn,flag;

// g2 = Z4 Z5 Z9 Z10; flag f1.
reset syn; reset flag; h flag;
cx data[3],syn; cx flag,syn; cx data[4],syn;
cx data[8],syn; cx flag,syn; cx data[9],syn;
m[2]=measure syn; h flag; f[1]=measure flag;
barrier data,syn,flag;

// g3 = Z6 Z7 Z11 Z12; flag f2.
reset syn; reset flag; h flag;
cx data[5],syn; cx flag,syn; cx data[6],syn;
cx data[10],syn; cx flag,syn; cx data[11],syn;
m[3]=measure syn; h flag; f[2]=measure flag;
barrier data,syn,flag;

// g4 = Z8 Z9 Z13 Z14; flag f3.
reset syn; reset flag; h flag;
cx data[7],syn; cx flag,syn; cx data[8],syn;
cx data[12],syn; cx flag,syn; cx data[13],syn;
m[4]=measure syn; h flag; f[3]=measure flag;
barrier data,syn,flag;

// g5 = Z10 Z15 (right boundary, weight 2; no flag required).
reset syn;
cx data[9],syn; cx data[14],syn;
m[5]=measure syn;
barrier data,syn,flag;

// g6 = Z11 Z16 (left boundary, weight 2; no flag required).
reset syn;
cx data[10],syn; cx data[15],syn;
m[6]=measure syn;
barrier data,syn,flag;

// g7 = Z12 Z13 Z17 Z18; flag f4.
reset syn; reset flag; h flag;
cx data[11],syn; cx flag,syn; cx data[12],syn;
cx data[16],syn; cx flag,syn; cx data[17],syn;
m[7]=measure syn; h flag; f[4]=measure flag;
barrier data,syn,flag;

// g8 = Z14 Z15 Z19 Z20; flag f5.
reset syn; reset flag; h flag;
cx data[13],syn; cx flag,syn; cx data[14],syn;
cx data[18],syn; cx flag,syn; cx data[19],syn;
m[8]=measure syn; h flag; f[5]=measure flag;
barrier data,syn,flag;

// g9 = Z16 Z17 Z21 Z22; flag f6.
reset syn; reset flag; h flag;
cx data[15],syn; cx flag,syn; cx data[16],syn;
cx data[20],syn; cx flag,syn; cx data[21],syn;
m[9]=measure syn; h flag; f[6]=measure flag;
barrier data,syn,flag;

// g10 = Z18 Z19 Z23 Z24; flag f7.
reset syn; reset flag; h flag;
cx data[17],syn; cx flag,syn; cx data[18],syn;
cx data[22],syn; cx flag,syn; cx data[23],syn;
m[10]=measure syn; h flag; f[7]=measure flag;
barrier data,syn,flag;

// g11 = Z20 Z25 (right boundary, weight 2; no flag required).
reset syn;
cx data[19],syn; cx data[24],syn;
m[11]=measure syn;
barrier data,syn,flag;

// g12 = X2 X3 (top boundary, weight 2; no flag required).
reset syn; h data[1]; h data[2];
cx data[1],syn; cx data[2],syn;
h data[1]; h data[2];
m[12]=measure syn;
barrier data,syn,flag;

// g13 = X4 X5 (top boundary, weight 2; no flag required).
reset syn; h data[3]; h data[4];
cx data[3],syn; cx data[4],syn;
h data[3]; h data[4];
m[13]=measure syn;
barrier data,syn,flag;

// g14 = X1 X2 X6 X7; flag f8.
reset syn; reset flag; h flag;
h data[0]; h data[1]; h data[5]; h data[6];
cx data[0],syn; cx flag,syn; cx data[1],syn;
cx data[5],syn; cx flag,syn; cx data[6],syn;
h data[0]; h data[1]; h data[5]; h data[6];
m[14]=measure syn; h flag; f[8]=measure flag;
barrier data,syn,flag;

// g15 = X3 X4 X8 X9; flag f9.
reset syn; reset flag; h flag;
h data[2]; h data[3]; h data[7]; h data[8];
cx data[2],syn; cx flag,syn; cx data[3],syn;
cx data[7],syn; cx flag,syn; cx data[8],syn;
h data[2]; h data[3]; h data[7]; h data[8];
m[15]=measure syn; h flag; f[9]=measure flag;
barrier data,syn,flag;

// g16 = X7 X8 X12 X13; flag f10.
reset syn; reset flag; h flag;
h data[6]; h data[7]; h data[11]; h data[12];
cx data[6],syn; cx flag,syn; cx data[7],syn;
cx data[11],syn; cx flag,syn; cx data[12],syn;
h data[6]; h data[7]; h data[11]; h data[12];
m[16]=measure syn; h flag; f[10]=measure flag;
barrier data,syn,flag;

// g17 = X9 X10 X14 X15; flag f11.
reset syn; reset flag; h flag;
h data[8]; h data[9]; h data[13]; h data[14];
cx data[8],syn; cx flag,syn; cx data[9],syn;
cx data[13],syn; cx flag,syn; cx data[14],syn;
h data[8]; h data[9]; h data[13]; h data[14];
m[17]=measure syn; h flag; f[11]=measure flag;
barrier data,syn,flag;

// g18 = X11 X12 X16 X17; flag f12.
reset syn; reset flag; h flag;
h data[10]; h data[11]; h data[15]; h data[16];
cx data[10],syn; cx flag,syn; cx data[11],syn;
cx data[15],syn; cx flag,syn; cx data[16],syn;
h data[10]; h data[11]; h data[15]; h data[16];
m[18]=measure syn; h flag; f[12]=measure flag;
barrier data,syn,flag;

// g19 = X13 X14 X18 X19; flag f13.
reset syn; reset flag; h flag;
h data[12]; h data[13]; h data[17]; h data[18];
cx data[12],syn; cx flag,syn; cx data[13],syn;
cx data[17],syn; cx flag,syn; cx data[18],syn;
h data[12]; h data[13]; h data[17]; h data[18];
m[19]=measure syn; h flag; f[13]=measure flag;
barrier data,syn,flag;

// g20 = X17 X18 X22 X23; flag f14.
reset syn; reset flag; h flag;
h data[16]; h data[17]; h data[21]; h data[22];
cx data[16],syn; cx flag,syn; cx data[17],syn;
cx data[21],syn; cx flag,syn; cx data[22],syn;
h data[16]; h data[17]; h data[21]; h data[22];
m[20]=measure syn; h flag; f[14]=measure flag;
barrier data,syn,flag;

// g21 = X19 X20 X24 X25; flag f15.
reset syn; reset flag; h flag;
h data[18]; h data[19]; h data[23]; h data[24];
cx data[18],syn; cx flag,syn; cx data[19],syn;
cx data[23],syn; cx flag,syn; cx data[24],syn;
h data[18]; h data[19]; h data[23]; h data[24];
m[21]=measure syn; h flag; f[15]=measure flag;
barrier data,syn,flag;

// g22 = X21 X22 (bottom boundary, weight 2; no flag required).
reset syn; h data[20]; h data[21];
cx data[20],syn; cx data[21],syn;
h data[20]; h data[21];
m[22]=measure syn;
barrier data,syn,flag;

// g23 = X23 X24 (bottom boundary, weight 2; no flag required).
reset syn; h data[22]; h data[23];
cx data[22],syn; cx data[23],syn;
h data[22]; h data[23];
m[23]=measure syn;
