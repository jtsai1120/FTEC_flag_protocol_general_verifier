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

- ordered SE events, including each SE's declared QASM filename, its `qd`/`qm`
  and optional `qf` quantum registers, and multiple invocations in one adaptive
  round;
- a `qasm_sequence` array containing the invoked QASM filenames in exact path
  execution order, including repetitions across adaptive and
  terminating-policy rounds;
- symbolic syndrome and flag names;
- the branch constraints imposed on those symbols, including the exact event
  checkpoint where each constraint was introduced;
- the selected terminal-condition and terminating-policy IDs;
- the terminal action (`decode` or `end`) and structured decoder record;
- bound and assertion status.

SE results use path-local sequential names: the first invocation produces
`id_1.s` and, for a flagged SE, `id_1.f`; the next invocation uses `id_2.s` and
`id_2.f`. Round, phase, and within-phase invocation numbers remain available as
separate event metadata instead of being embedded in the symbolic identifier.

SE results are symbolic rather than enumerated bit patterns. Conditions that
choose different control flow fork the path array. Multiple symbolic witnesses
with the same subsequent control flow remain one compound path condition. For
example, `!(A and B)` stays one `false` outcome rather than becoming separate
`!A` and `A and !B` paths.

Before expansion, the parser computes a backward control-relevance slice from
terminal conditions and SE invocations. A condition becomes a path branch only
if it can change a later SE sequence or terminal-policy selection. Record-only
control flow is evaluated abstractly without forking; differing data values are
retained as `ite(...)` expressions, and a data-only loop result is represented
as `loop_result(...)` in the structured decode record.

The parser uses a private, ordinary syntax tree only while parsing and executing
the program. That tree is neither shared nor returned to the caller.

## Current constraint reasoning

The executor constant-folds counters and Boolean expressions and runs an
internal propositional satisfiability check across compound `and`/`or`
constraints. This rejects combinations such as `!(A and B)`, `A`, and `B`
without splitting one control-flow outcome into multiple paths. It does not yet
call a bit-vector SMT solver, so more complex algebraic relations may still
require downstream pruning.

## Draw a path graph from JSON

`fpdl-path-graph` reads the symbolic-path JSON emitted by `fpdlc` and creates a
common-prefix graph. Shared SE event prefixes are drawn once, branch constraints
are diamonds, and every leaf shows its path ID and termination policy.

Create a standalone SVG (Graphviz is not required):

```sh
./build/fpdl-path-graph protocol.paths.json -o docs/samples/protocol.paths.svg
```

Create Graphviz DOT instead:

```sh
./build/fpdl-path-graph protocol.paths.json -o docs/samples/protocol.paths.dot
```

Useful options:

- `--max-paths N` limits the number of rendered paths (default: 100). The tool
  prints a warning and marks the SVG header when the render is truncated.
- `--hide-constraints` omits constraint diamonds for a more compact event-only
  graph.
- `--format svg|dot` selects the format explicitly; otherwise the output file
  extension selects it.

Each new constraint includes `after_event`, `round`, and `phase` metadata. The
graph therefore places a decision at its actual control-flow checkpoint even
when the expression references an SE result from an earlier round. For backward
compatibility, JSON without `after_event` still uses symbolic-value references
to infer a best-effort location.

## Build a control-flow DAG from JSON

`fpdl-path-dag` converts symbolic paths into a shared-prefix DAG encoded as
JSON. SE nodes contain the declared QASM filename. Conditions are stored on
edges; an edge without an additional branch constraint has
`"condition": "true"`.

```sh
./build/fpdl-path-dag protocol.paths.json \
  --max-paths 5000 -o protocol.dag.json
```

The input should be regenerated with the current `fpdlc` to include
`qasm_file`, `data_register`, and `flag_register`. Older JSON remains accepted,
but metadata absent from that input is emitted as `null`.

### Visualize the DAG JSON

`fpdl-dag-graph` renders `protocol.dag.json` as a standalone SVG. SE states show
the QASM filename, data/flag registers, round, phase, and invocation; each edge
is labeled with its control-flow condition.

```sh
./build/fpdl-dag-graph protocol.dag.json -o protocol.dag.svg
```

DOT output is also available:

```sh
./build/fpdl-dag-graph protocol.dag.json \
  --format dot -o protocol.dag.dot
```

Use `--hide-true-conditions` to omit labels from unconditional edges. The tool
rejects duplicate node IDs, unknown edge endpoints, unsupported state kinds,
and directed cycles.
