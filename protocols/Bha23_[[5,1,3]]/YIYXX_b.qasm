OPENQASM 3.0;
include "stdgates.inc";

// Unflagged syndrome extraction for G5 = YIYXX.
//
// data[0] = q1, data[1] = q2, data[2] = q3,
// data[3] = q4, data[4] = q5.
//
// syn = syndrome ancilla, initialized to |0>.
// m = syndrome measurement bit.

qubit[5] data;
qubit syn;

bit m;

// Measure a Z component using syn initialized in |0>:
// data qubit is control, syndrome ancilla is target.
gate meas_z_component d, a {
    cx d, a;
}

// Measure an X component by changing the data-qubit basis:
// H; CNOT(data -> ancilla); H.
gate meas_x_component d, a {
    h d;
    cx d, a;
    h d;
}

// Measure a Y component by changing the data-qubit basis:
// S^dagger H; CNOT(data -> ancilla); H S.
gate meas_y_component d, a {
    sdg d;
    h d;
    cx d, a;
    h d;
    s d;
}


// ------------------------------------------------------------
// G5 = YIYXX
// ------------------------------------------------------------

reset syn;

// Y on q1
meas_y_component data[0], syn;

// Y on q3
meas_y_component data[2], syn;

// X on q4
meas_x_component data[3], syn;

// X on q5
meas_x_component data[4], syn;

// syndrome Z-basis measurement
m = measure syn;
