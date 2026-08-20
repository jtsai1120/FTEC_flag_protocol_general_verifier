OPENQASM 3.0;
include "stdgates.inc";

// data[0] = q1, data[1] = q2, data[2] = q3,
// data[3] = q4, data[4] = q5.
//
// syn  = syndrome ancilla, initialized to |0>
// flag = flag ancilla, initialized to |+>
//
// m = syndrome bit
// f = flag bit

qubit[5] data;
qubit syn;
qubit flag;

bit m;
bit f;

// Measure a Z component using syn initialized in |0>:
// data qubit is control, syndrome ancilla is target.
gate meas_z_component d, a {
    cx d, a;
}

// Measure an X component using basis change:
// H; CNOT(data -> ancilla); H.
gate meas_x_component d, a {
    h d;
    cx d, a;
    h d;
}

// ------------------------------------------------------------
// Flagged syndrome extraction for G0 = X Z Z X I
// ------------------------------------------------------------

reset syn;
reset flag;
h flag;              // prepare |+> flag

// a: X on q1
meas_x_component data[0], syn;

// couple syndrome ancilla to flag
cx flag, syn;

// b: Z on q2
meas_z_component data[1], syn;

// c: Z on q3
meas_z_component data[2], syn;

// couple syndrome ancilla to flag
cx flag, syn;

// d: X on q4
meas_x_component data[3], syn;

// syndrome Z-basis measurement
m = measure syn;

// flag X-basis measurement
h flag;
f = measure flag;
