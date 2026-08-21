#include "fpdl/parser.hpp"

#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

bool rejects(const std::string& source) {
    fpdl::ParseOptions options;
    options.inspect_qasm_registers = false;
    options.bmc_bound = 32;
    options.max_paths = 1000;
    try {
        (void)fpdl::Parser::parse_string(source, "invalid.fpdl", options);
    } catch (const fpdl::ParseError&) {
        return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "expected at least one .fpdl example\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        try {
            fpdl::ParseOptions options;
            options.bmc_bound = 48;
            options.max_paths = 5000;
            const auto result = fpdl::Parser::parse_file(argv[i], options);
            if (result.protocol_name.empty() || result.paths.empty()) {
                std::cerr << argv[i] << ": parser did not produce symbolic paths\n";
                return 1;
            }
            const auto json = result.to_json();
            if (json.find("\"protocol\"") == std::string::npos ||
                json.find("\"paths\"") == std::string::npos ||
                json.find("\"events\"") == std::string::npos) {
                std::cerr << argv[i] << ": invalid symbolic-path JSON\n";
                return 1;
            }
        } catch (const std::exception& error) {
            std::cerr << argv[i] << ": " << error.what() << '\n';
            return 1;
        }
    }

    if (!rejects("protocol bad: code: [[1,1,1]] [[I]] se: gvar: "
                 "bit x = 0 tc: 1: true af: while(!tc) {} tp: 1: { end(); }")) {
        std::cerr << "invalid declaration without semicolon was accepted\n";
        return 1;
    }

    const std::string prefix =
        "protocol bad: code: [[1,1,1]] [[I]] se: "
        "g { file: \"x.qasm\" qp: q qm: q cm: c g: [[I]] } ";
    if (!rejects(prefix +
                 "gvar: bit s = 0; tc: 1: true "
                 "af: while(!tc) { (s) = g(); } tp: 1: { end(); }")) {
        std::cerr << "old singleton tuple syntax was accepted\n";
        return 1;
    }

    if (!rejects("protocol bad: code: [[1,1,1]] [[I]] se: gvar: bit x = 0; "
                 "tc: 1: local == 0 af: while(!tc) {} tp: 1: { end(); }")) {
        std::cerr << "terminal condition referencing a nonglobal was accepted\n";
        return 1;
    }

    if (!rejects("protocol bad: code: [[1,1,1]] [[I]] se: gvar: bit x = 0; "
                 "tc: 1: true af: while(!tc) {} tp: 1: { check(); }")) {
        std::cerr << "check() in terminating policy was accepted\n";
        return 1;
    }

    fpdl::ParseOptions symbolic_options;
    symbolic_options.inspect_qasm_registers = false;
    symbolic_options.bmc_bound = 40;
    const auto symbolic = fpdl::Parser::parse_string(
        "protocol branching: code: [[1,1,1]] [[I]] se: "
        "first { file: \"first.qasm\" qp: q qm: q cm: m qf: qf cf: f g: [[I]] #-flag: 1 } "
        "left { file: \"left.qasm\" qp: q qm: q cm: m g: [[I]] } "
        "right { file: \"right.qasm\" qp: q qm: q cm: m g: [[I]] } "
        "gvar: cnt n = 0; bit s = 0; bit f = 0; bit x = 0; "
        "tc: 1: n == 2 "
        "af: while(!tc) { "
        "  if (n == 0) { (s, f) = first(); } "
        "  else if (s == 0) { (x,) = left(); } "
        "  else { (x,) = right(); } "
        "  n++; "
        "} "
        "tp: 1: { end(); }",
        "branching.fpdl", symbolic_options);
    std::set<std::string> second_calls;
    for (const auto& path : symbolic.paths) {
        if (path.events.size() >= 2) second_calls.insert(path.events[1].se_name);
    }
    if (!second_calls.contains("left") || !second_calls.contains("right")) {
        std::cerr << "symbolic SE result did not select both downstream SE paths\n";
        return 1;
    }

    return 0;
}
