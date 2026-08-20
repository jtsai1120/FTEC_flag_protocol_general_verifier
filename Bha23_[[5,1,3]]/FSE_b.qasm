OPENQASM 3.0;
include "stdgates.inc";

// Combined unflagged syndrome extraction for the [[5,1,3]] generators,
// in the current order:
//   G0 = X Z Z X I
//   G1 = I X Z Z X
//   G2 = X I X Z Z
//   G3 = Z X I X Z
//
// data[0] = q1, data[1] = q2, data[2] = q3,
// data[3] = q4, data[4] = q5.
//
// syn  = syndrome ancilla, initialized to |0>
// m[i] = syndrome bit for generator Gi

qubit[5] data;
qubit syn;

bit[4] m;

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
// Unflagged SE for G0 = X Z Z X I
// ------------------------------------------------------------

reset syn;

// X on q1
meas_x_component data[0], syn;

// Z on q2
meas_z_component data[1], syn;

// Z on q3
meas_z_component data[2], syn;

// X on q4
meas_x_component data[3], syn;

// syndrome Z-basis measurement
m[0] = measure syn;

barrier data, syn;


// ------------------------------------------------------------
// Unflagged SE for G1 = I X Z Z X
// ------------------------------------------------------------

reset syn;

// X on q2
meas_x_component data[1], syn;

// Z on q3
meas_z_component data[2], syn;

// Z on q4
meas_z_component data[3], syn;

// X on q5
meas_x_component data[4], syn;

// syndrome Z-basis measurement
m[1] = measure syn;

barrier data, syn;


// ------------------------------------------------------------
// Unflagged SE for G2 = X I X Z Z
// ------------------------------------------------------------

reset syn;

// X on q1
meas_x_component data[0], syn;

// X on q3
meas_x_component data[2], syn;

// Z on q4
meas_z_component data[3], syn;

// Z on q5
meas_z_component data[4], syn;

// syndrome Z-basis measurement
m[2] = measure syn;

barrier data, syn;


// ------------------------------------------------------------
// Unflagged SE for G3 = Z X I X Z
// ------------------------------------------------------------

reset syn;

// Z on q1
meas_z_component data[0], syn;

// X on q2
meas_x_component data[1], syn;

// X on q4
meas_x_component data[3], syn;

// Z on q5
meas_z_component data[4], syn;

// syndrome Z-basis measurement
m[3] = measure syn;
