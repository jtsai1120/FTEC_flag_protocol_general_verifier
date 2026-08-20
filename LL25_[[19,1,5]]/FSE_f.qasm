OPENQASM 3.0;
include "stdgates.inc";

// Fully parallel flagged syndrome extraction for the [[19,1,5]] CSS code
// (Liou & Lai, arXiv:2407.00607, the '1' scheme of Sec. III B / Fig. 1(b)).
//
// Each of the 18 stabilizer generators keeps its OWN independent measurement
// qubit and its own w/2-1 flag qubits (the per-generator flag pattern follows
// the minimal-flag-qubit construction of Chamberland & Beverland, arXiv:1708.02246,
// verified against this codebase's existing weight-4/weight-8 examples). What makes
// this THE FULLY PARALLEL SCHEME (rather than a serial one) is that CNOTs from
// different generators are interleaved in time, subject to two constraints proven
// sufficient for fault tolerance by Lemma 6 of arXiv:2407.00607:
//   (1) within one generator, its own CNOT order is preserved verbatim;
//   (2) for any data qubit shared by two generators, the relative order of their
//       touches to that qubit matches the order of g1..g18 (the reference serial
//       scheme) -- i.e. no reordering across generators on a shared data qubit.
// A generator's measurement/flag qubits are exclusive to it (this is the 'fully
// parallel' scheme, NOT the flag-SHARING scheme -- see note at the end of file).
//
// data[0] = q1, ..., data[18] = q19.
// syn[0..8]  = one measurement qubit per generator within a block (reused between
//              the Z-block and the X-block).
// flag[0..11] = 12 flag qubits per block (6 weight-4 generators x 1 flag + 3
//               weight-6 generators x 2 flags = 12; reused between blocks).
// m[0..8]  = syndrome bits for g1..g9 (Z-type).
// m[9..17] = syndrome bits for g10..g18 (X-type).
// f[0..11]  = flag outcomes for g1..g9, in the slot order:
//             g1->f[0], g2->f[1], g3->f[2], g4->f[3,4], g5->f[5], g6->f[6],
//             g7->f[7], g8->f[8,9], g9->f[10,11].
// f[12..23] = flag outcomes for g10..g18, same slot pattern offset by 12.

qubit[19] data;
qubit[9]  syn;
qubit[12] flag;

bit[18] m;
bit[24] f;

// ------------------------------------------------------------
// Z-type stabilizers g1..g9 (fully parallel): 9 stabilizers, 12 flag qubits, depth 22
// ------------------------------------------------------------
reset syn[0];
reset syn[1];
reset syn[2];
reset syn[3];
reset syn[4];
reset syn[5];
reset syn[6];
reset syn[7];
reset syn[8];
reset flag[0];
h flag[0];  // prepare |+>
reset flag[1];
h flag[1];  // prepare |+>
reset flag[2];
h flag[2];  // prepare |+>
reset flag[3];
h flag[3];  // prepare |+>
reset flag[4];
h flag[4];  // prepare |+>
reset flag[5];
h flag[5];  // prepare |+>
reset flag[6];
h flag[6];  // prepare |+>
reset flag[7];
h flag[7];  // prepare |+>
reset flag[8];
h flag[8];  // prepare |+>
reset flag[9];
h flag[9];  // prepare |+>
reset flag[10];
h flag[10];  // prepare |+>
reset flag[11];
h flag[11];  // prepare |+>

// -- depth 1 --
cx data[0], syn[0];
cx data[11], syn[2];
cx data[9], syn[6];

// -- depth 2 --
cx data[0], syn[1];
cx flag[0], syn[0];
cx flag[2], syn[2];
cx flag[7], syn[6];

// -- depth 3 --
cx data[0], syn[3];
cx flag[1], syn[1];
cx data[1], syn[0];
cx data[12], syn[2];
cx data[10], syn[6];

// -- depth 4 --
cx flag[3], syn[3];
cx data[2], syn[0];
cx data[13], syn[2];
cx data[11], syn[6];

// -- depth 5 --
cx data[1], syn[3];
cx data[2], syn[1];
cx flag[0], syn[0];
cx flag[2], syn[2];
cx flag[7], syn[6];
h flag[0];
f[0] = measure flag[0];
h flag[2];
f[2] = measure flag[2];
h flag[7];
f[7] = measure flag[7];

// -- depth 6 --
cx flag[4], syn[3];
cx data[4], syn[1];
cx data[3], syn[0];
cx data[14], syn[2];
m[0] = measure syn[0];
m[2] = measure syn[2];

// -- depth 7 --
cx data[4], syn[3];
cx flag[1], syn[1];
cx data[14], syn[6];
m[6] = measure syn[6];
h flag[1];
f[1] = measure flag[1];

// -- depth 8 --
cx data[4], syn[8];
cx data[5], syn[3];
cx data[6], syn[1];
m[1] = measure syn[1];

// -- depth 9 --
cx flag[10], syn[8];
cx data[5], syn[4];
cx flag[3], syn[3];
h flag[3];
f[3] = measure flag[3];

// -- depth 10 --
cx data[6], syn[8];
cx flag[5], syn[4];
cx data[7], syn[3];

// -- depth 11 --
cx data[7], syn[7];
cx flag[11], syn[8];
cx flag[4], syn[3];
h flag[4];
f[4] = measure flag[4];

// -- depth 12 --
cx flag[8], syn[7];
cx data[7], syn[8];
cx data[8], syn[3];
m[3] = measure syn[3];

// -- depth 13 --
cx data[8], syn[4];

// -- depth 14 --
cx data[8], syn[7];
cx data[15], syn[4];

// -- depth 15 --
cx flag[9], syn[7];
cx data[15], syn[5];
cx flag[5], syn[4];
h flag[5];
f[5] = measure flag[5];

// -- depth 16 --
cx data[9], syn[7];
cx flag[6], syn[5];
cx data[18], syn[4];
m[4] = measure syn[4];

// -- depth 17 --
cx data[10], syn[7];
cx data[16], syn[5];

// -- depth 18 --
cx data[10], syn[8];
cx flag[8], syn[7];
cx data[17], syn[5];
h flag[8];
f[8] = measure flag[8];

// -- depth 19 --
cx flag[10], syn[8];
cx data[15], syn[7];
cx flag[6], syn[5];
h flag[10];
f[10] = measure flag[10];
h flag[6];
f[6] = measure flag[6];

// -- depth 20 --
cx data[11], syn[8];
cx flag[9], syn[7];
cx data[18], syn[5];
m[5] = measure syn[5];
h flag[9];
f[9] = measure flag[9];

// -- depth 21 --
cx flag[11], syn[8];
cx data[16], syn[7];
m[7] = measure syn[7];
h flag[11];
f[11] = measure flag[11];

// -- depth 22 --
cx data[12], syn[8];
m[8] = measure syn[8];

barrier data, syn, flag;

// ------------------------------------------------------------
// X-type stabilizers g10..g18 (fully parallel): 9 stabilizers, 12 flag qubits, depth 22
// ------------------------------------------------------------
reset syn[0];
reset syn[1];
reset syn[2];
reset syn[3];
reset syn[4];
reset syn[5];
reset syn[6];
reset syn[7];
reset syn[8];
reset flag[0];
h flag[0];  // prepare |+>
reset flag[1];
h flag[1];  // prepare |+>
reset flag[2];
h flag[2];  // prepare |+>
reset flag[3];
h flag[3];  // prepare |+>
reset flag[4];
h flag[4];  // prepare |+>
reset flag[5];
h flag[5];  // prepare |+>
reset flag[6];
h flag[6];  // prepare |+>
reset flag[7];
h flag[7];  // prepare |+>
reset flag[8];
h flag[8];  // prepare |+>
reset flag[9];
h flag[9];  // prepare |+>
reset flag[10];
h flag[10];  // prepare |+>
reset flag[11];
h flag[11];  // prepare |+>

// -- depth 1 --
h data[0];  // basis change (X-type)
h data[11];  // basis change (X-type)
h data[9];  // basis change (X-type)
cx data[0], syn[0];
cx data[11], syn[2];
cx data[9], syn[6];

// -- depth 2 --
cx data[0], syn[1];
cx flag[0], syn[0];
cx flag[2], syn[2];
cx flag[7], syn[6];

// -- depth 3 --
h data[1];  // basis change (X-type)
h data[12];  // basis change (X-type)
h data[10];  // basis change (X-type)
cx data[0], syn[3];
cx flag[1], syn[1];
cx data[1], syn[0];
cx data[12], syn[2];
cx data[10], syn[6];
h data[0];  // restore basis

// -- depth 4 --
h data[2];  // basis change (X-type)
h data[13];  // basis change (X-type)
cx flag[3], syn[3];
cx data[2], syn[0];
cx data[13], syn[2];
cx data[11], syn[6];
h data[13];  // restore basis

// -- depth 5 --
cx data[1], syn[3];
cx data[2], syn[1];
cx flag[0], syn[0];
cx flag[2], syn[2];
cx flag[7], syn[6];
h data[1];  // restore basis
h data[2];  // restore basis
h flag[0];
f[12] = measure flag[0];
h flag[2];
f[14] = measure flag[2];
h flag[7];
f[19] = measure flag[7];

// -- depth 6 --
h data[4];  // basis change (X-type)
h data[3];  // basis change (X-type)
h data[14];  // basis change (X-type)
cx flag[4], syn[3];
cx data[4], syn[1];
cx data[3], syn[0];
cx data[14], syn[2];
h data[3];  // restore basis
m[9] = measure syn[0];
m[11] = measure syn[2];

// -- depth 7 --
cx data[4], syn[3];
cx flag[1], syn[1];
cx data[14], syn[6];
h data[14];  // restore basis
m[15] = measure syn[6];
h flag[1];
f[13] = measure flag[1];

// -- depth 8 --
h data[5];  // basis change (X-type)
h data[6];  // basis change (X-type)
cx data[4], syn[8];
cx data[5], syn[3];
cx data[6], syn[1];
h data[4];  // restore basis
m[10] = measure syn[1];

// -- depth 9 --
cx flag[10], syn[8];
cx data[5], syn[4];
cx flag[3], syn[3];
h data[5];  // restore basis
h flag[3];
f[15] = measure flag[3];

// -- depth 10 --
h data[7];  // basis change (X-type)
cx data[6], syn[8];
cx flag[5], syn[4];
cx data[7], syn[3];
h data[6];  // restore basis

// -- depth 11 --
cx data[7], syn[7];
cx flag[11], syn[8];
cx flag[4], syn[3];
h flag[4];
f[16] = measure flag[4];

// -- depth 12 --
h data[8];  // basis change (X-type)
cx flag[8], syn[7];
cx data[7], syn[8];
cx data[8], syn[3];
h data[7];  // restore basis
m[12] = measure syn[3];

// -- depth 13 --
cx data[8], syn[4];

// -- depth 14 --
h data[15];  // basis change (X-type)
cx data[8], syn[7];
cx data[15], syn[4];
h data[8];  // restore basis

// -- depth 15 --
cx flag[9], syn[7];
cx data[15], syn[5];
cx flag[5], syn[4];
h flag[5];
f[17] = measure flag[5];

// -- depth 16 --
h data[18];  // basis change (X-type)
cx data[9], syn[7];
cx flag[6], syn[5];
cx data[18], syn[4];
h data[9];  // restore basis
m[13] = measure syn[4];

// -- depth 17 --
h data[16];  // basis change (X-type)
cx data[10], syn[7];
cx data[16], syn[5];

// -- depth 18 --
h data[17];  // basis change (X-type)
cx data[10], syn[8];
cx flag[8], syn[7];
cx data[17], syn[5];
h data[10];  // restore basis
h data[17];  // restore basis
h flag[8];
f[20] = measure flag[8];

// -- depth 19 --
cx flag[10], syn[8];
cx data[15], syn[7];
cx flag[6], syn[5];
h data[15];  // restore basis
h flag[10];
f[22] = measure flag[10];
h flag[6];
f[18] = measure flag[6];

// -- depth 20 --
cx data[11], syn[8];
cx flag[9], syn[7];
cx data[18], syn[5];
h data[11];  // restore basis
h data[18];  // restore basis
m[14] = measure syn[5];
h flag[9];
f[21] = measure flag[9];

// -- depth 21 --
cx flag[11], syn[8];
cx data[16], syn[7];
h data[16];  // restore basis
m[16] = measure syn[7];
h flag[11];
f[23] = measure flag[11];

// -- depth 22 --
cx data[12], syn[8];
h data[12];  // restore basis
m[17] = measure syn[8];


// NOTE on scope: this circuit realises the FULLY PARALLEL ('1') scheme of
// arXiv:2407.00607, which the paper proves fault-tolerant unconditionally (Lemma 6)
// given a fault-tolerant serial scheme as the starting point. It does NOT realise
// the FLAG-SHARING '[2;2;2;1;1;1]' scheme (Fig. 22 of the paper), which additionally
// merges several generators onto a SINGLE shared flag qubit; that scheme's exact
// CNOT arrangement is only given as a circuit diagram in the paper (not recoverable
// from body text alone) and would need independent re-verification of Prop. 9's
// distinguishability condition, so it is not reproduced here.
//
// Depth note: the greedy interleaving used to build this file achieves circuit
// depth 22 per block (vs. 66 for a fully serial schedule) -- a genuine ~3x depth
// reduction from real concurrent execution, though not the paper's fully optimised
// depth of 10, which additionally re-orders the CNOTs WITHIN each generator (Remark 8)
// in a way that needs re-checking the fault-tolerance condition per reordering.
