OPENQASM 3.0;
include "stdgates.inc";

// Combined serial unflagged extraction for the same Figure-8 [[25,1,5]]
// rotated surface-code generators and syndrome-bit order as FSE_f.qasm.
qubit[25] data;
qubit syn;
bit[24] m;

// Z checks g0..g11.
reset syn; cx data[0],syn; cx data[5],syn; m[0]=measure syn; barrier data,syn;

reset syn; cx data[1],syn; cx data[2],syn; cx data[6],syn; cx data[7],syn;
m[1]=measure syn; barrier data,syn;

reset syn; cx data[3],syn; cx data[4],syn; cx data[8],syn; cx data[9],syn;
m[2]=measure syn; barrier data,syn;

reset syn; cx data[5],syn; cx data[6],syn; cx data[10],syn; cx data[11],syn;
m[3]=measure syn; barrier data,syn;

reset syn; cx data[7],syn; cx data[8],syn; cx data[12],syn; cx data[13],syn;
m[4]=measure syn; barrier data,syn;

reset syn; cx data[9],syn; cx data[14],syn; m[5]=measure syn; barrier data,syn;

reset syn; cx data[10],syn; cx data[15],syn; m[6]=measure syn; barrier data,syn;

reset syn; cx data[11],syn; cx data[12],syn; cx data[16],syn; cx data[17],syn;
m[7]=measure syn; barrier data,syn;

reset syn; cx data[13],syn; cx data[14],syn; cx data[18],syn; cx data[19],syn;
m[8]=measure syn; barrier data,syn;

reset syn; cx data[15],syn; cx data[16],syn; cx data[20],syn; cx data[21],syn;
m[9]=measure syn; barrier data,syn;

reset syn; cx data[17],syn; cx data[18],syn; cx data[22],syn; cx data[23],syn;
m[10]=measure syn; barrier data,syn;

reset syn; cx data[19],syn; cx data[24],syn; m[11]=measure syn; barrier data,syn;

// X checks g12..g23.  Single-qubit Clifford basis changes follow Figure 2.
reset syn; h data[1]; h data[2]; cx data[1],syn; cx data[2],syn;
h data[1]; h data[2]; m[12]=measure syn; barrier data,syn;

reset syn; h data[3]; h data[4]; cx data[3],syn; cx data[4],syn;
h data[3]; h data[4]; m[13]=measure syn; barrier data,syn;

reset syn; h data[0]; h data[1]; h data[5]; h data[6];
cx data[0],syn; cx data[1],syn; cx data[5],syn; cx data[6],syn;
h data[0]; h data[1]; h data[5]; h data[6]; m[14]=measure syn; barrier data,syn;

reset syn; h data[2]; h data[3]; h data[7]; h data[8];
cx data[2],syn; cx data[3],syn; cx data[7],syn; cx data[8],syn;
h data[2]; h data[3]; h data[7]; h data[8]; m[15]=measure syn; barrier data,syn;

reset syn; h data[6]; h data[7]; h data[11]; h data[12];
cx data[6],syn; cx data[7],syn; cx data[11],syn; cx data[12],syn;
h data[6]; h data[7]; h data[11]; h data[12]; m[16]=measure syn; barrier data,syn;

reset syn; h data[8]; h data[9]; h data[13]; h data[14];
cx data[8],syn; cx data[9],syn; cx data[13],syn; cx data[14],syn;
h data[8]; h data[9]; h data[13]; h data[14]; m[17]=measure syn; barrier data,syn;

reset syn; h data[10]; h data[11]; h data[15]; h data[16];
cx data[10],syn; cx data[11],syn; cx data[15],syn; cx data[16],syn;
h data[10]; h data[11]; h data[15]; h data[16]; m[18]=measure syn; barrier data,syn;

reset syn; h data[12]; h data[13]; h data[17]; h data[18];
cx data[12],syn; cx data[13],syn; cx data[17],syn; cx data[18],syn;
h data[12]; h data[13]; h data[17]; h data[18]; m[19]=measure syn; barrier data,syn;

reset syn; h data[16]; h data[17]; h data[21]; h data[22];
cx data[16],syn; cx data[17],syn; cx data[21],syn; cx data[22],syn;
h data[16]; h data[17]; h data[21]; h data[22]; m[20]=measure syn; barrier data,syn;

reset syn; h data[18]; h data[19]; h data[23]; h data[24];
cx data[18],syn; cx data[19],syn; cx data[23],syn; cx data[24],syn;
h data[18]; h data[19]; h data[23]; h data[24]; m[21]=measure syn; barrier data,syn;

reset syn; h data[20]; h data[21]; cx data[20],syn; cx data[21],syn;
h data[20]; h data[21]; m[22]=measure syn; barrier data,syn;

reset syn; h data[22]; h data[23]; cx data[22],syn; cx data[23],syn;
h data[22]; h data[23]; m[23]=measure syn;
