# C++ FPDL parser and symbolic path expander

Build and test:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Parse an FPDL file, symbolically execute its control flow, and emit a JSON array
of paths:

```sh
./build/fpdlc CR17_\[\[5,1,3\]\]/CR17_\[\[5,1,3\]\].fpdl \
  --bmc-bound 128 --max-paths 1000 -o protocol.paths.json
```

`--bmc-bound` is required and limits the transition count of each path.
`--max-paths` prevents path explosion; the default is 1,000. If the limit is
reached, the result has `"truncated": true` and the CLI emits a warning.

## Path representation

The public C++ result is `std::vector<fpdl::SymbolicPath>`. No DAG or
hash-consing is used. Every path contains:

- ordered SE events, including multiple invocations in one adaptive round;
- symbolic syndrome and flag names;
- the branch constraints imposed on those symbols;
- the selected terminal-condition and terminating-policy IDs;
- the terminal action (`decode` or `end`) and structured decoder record;
- bound and assertion status.

SE results are symbolic rather than enumerated bit patterns. Conditions that
choose different control flow fork the path array. Conditions with the same
control-flow consequence remain a single symbolic constraint, avoiding needless
duplication for every concrete witness.

The parser uses a private, ordinary syntax tree only while parsing and executing
the program. That tree is neither shared nor returned to the caller.

## Current constraint reasoning

The executor constant-folds counters and Boolean expressions, expands the
short-circuit cases needed to follow control flow, and rejects exact opposite
constraints. It does not yet call an SMT solver, so the CLI warns that more
complex algebraically inconsistent paths may still require downstream pruning.
