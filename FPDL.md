# FPDL — Flag Protocol Description Language

FPDL describes a QECC **flag protocol**, and a `.fpdl` file has the following form:

```
protocol <protocol_name>:
    code: ...
    se:   ...
    gvar: ...
    tc:   ...
    af:   ...
    tp:   ...
```

---

## `code` — QEC Code

```
code:
    [[<n>, <k>, <d>]]                       # physical qubit num, logical qubit num, code distance
    [[<g1>], [<g2>], ... ]                  # stabilizer generators
```

A generator `<gi>` can be written either:

- **dense** — e.g. `[[I, X, X, Z], [Z, X, X, I], [I, Z, Z, Z]]`
- **sparse** — e.g. `[[X3, X4, X5, X6], [X14, X15, X16, X17], [Z1, Z2, Z3, Z4]]`

---

## `se` — syndrome-extraction circuits

```
se:
    <se_name> {
        file: <OpenQASM_file_path>
        qp: <physical_qreg>
        qm: <measurement_qreg>
        cm: <measurement_creg>
        qf: <flag_qreg>                     # only specify if se is flagged
        cf: <flag_creg>                     # only specify if se is flagged
        g:  <generator_set>                 # claimed measured generator(s)
        #-flag: <t>                         # claimed #-flag circuit,
                                              only specify if se is flagged
    }
```

Each `se_name` can be called from `af` as a **black-boxed, atomic** unit: `af` invokes
`se_name()`, receives its outcome (`s`, or `{s,f}` if flagged).

> **Note:**  
> `#-flag` does not denote the # of flag qubit, it is defined in a sence of any `0 < v ≤ #-flag` fault assignment in the se must flag (any of flag qubit raises) if the error propagated by that `v` faults has a minimum weight greater then `v`. (i.e. flag circuit is claimed to function correctly, which is to detect high-weight propagation error, up to `#-flag` faults)

---

## `gvar` — global variables
Global variables are either used to represent the current state of the adaptive flow, or used only to memorize the measurement record.

```
gvar:
    <data_type> <var_name>;                 # w/o initial value assignment (i.e. assign ⊥)
    <data_type> <var_name> = <init_value>;  # w/ initial value assignment
```

### <data_type>

| Type | Domain | Explain |
|---|---|---|
| `bit` | `{⊥} ∪ {0, 1}` |a boolean|
| `cnt` | `{⊥} ∪ {0, 1, 2, 3, ...}` |a integer counter|
| `mr`  | `{⊥} ∪ (bit or bit[] or cnt)[]` |a measurement record|
| `se`  | `{⊥} ∪ se` |a se circuit|

> **Note:**   
> `⊥` is a **monolithic** symbol representing that the variable is ***unassigned***.  
> (Partial-element unassignment in a vector data type is not valid currently.)

Vector variable can also be declared as: `<data_type>[<vector_length>]`, for exmaple:
- `bit[3]` (bit-vector w/ fixed length 3)
- `bit[]` (bit-vector w/ arbitrary length)
- `bit[][]` (vector w/ arbitrary length of bit-vectors w/ arbitrary length)
- `se[]` (`se`-vector w/ arbitrary length)

A vector is represented by brackets enclosing elements. (e.g. `bit[6] a = [0,1,0,1,1,0]`)

Concatenation of two vectors can be declared with braces, for example:  
If `a, b` are both `bit[3]`, then `{a,b}` returns a bit-vector w/ length 6 concatenating a and b.

---

## `tc` — terminal conditions

```
tc:
    <tc_num>: <bool_expr>
```

If any `tc_num`'s condition holds, `af`'s loop terminates and `tp[tc_num]` runs.

### <bool_expr> 
A boolean expression is described by
- Two variables with same data_type and one of the binary operator: `==`, `!=`, `>`, `<`, `>=`, `<=`
- Two bool_expr with binary operators: `and`, `or`
- tautology: `true`, `false`

> **Note:**
> for a bit-vector, `0` can denote the all-zero vector for conveniece.

---

## `af` — adaptive flow

```
af:
    while(!tc) {
        <high-level language description>
    }
```

### HLL control-flow expression

```
if (<bool_expr>) { <execution>; }
else if (<bool_expr>) { <execution>; }
else { <execution>; }

switch (<variable>) {
    <value1>: { <execution>; }
    <value2>: { <execution>; }
    ...
    default: { <execution>; }
}

while (<bool_expr>) { <execution>; }
```

### Execution primitives

- `{s,f} = g1()` — execute se `g1`, then store the syndrome result in `s` and the flag result in `f`
- `a = b;` — assign `b` to `a` (only valid when `a` and `b` has same data type)
- `a++` — add 1 to `a` (only valid for data_type `cnt`)
- `a += b` — add `b` to `a` (only valid when both `a` and `b` has data_type `cnt`)
- `a.push(b)` — if `a` is a vector, append element `b`
- `break` — break out of the enclosing `for` loop
- `continue` — continue the enclosing `for` or `while` loop

---

## `tp` — terminating policy

```
tp:
    <tc_num>: { <terminate_execution>; }
```

Each terminating policy must end with exactly one of the following lines:

- `decode(<lookup_vector>);` — `<lookup_vector>` must have type `mr`, representing a LUT decoder is implemented.  

    > **Note:**   
    > `decode()` is regarded as an **external black-box** currently. It's still an open 
    > question to construct a well-formed description language for the decoding algorithm.
- `end();` — explicitly declares that no decoding is needed


