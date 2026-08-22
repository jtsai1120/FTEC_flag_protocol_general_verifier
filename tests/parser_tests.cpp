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
            if (std::string(argv[i]).find("CB18_[[17,1,5]]_plain") != std::string::npos) {
                for (const auto& path : result.paths) {
                    for (const auto& constraint : path.constraints) {
                        if (constraint.expression.find(".f[") != std::string::npos) {
                            std::cerr << argv[i]
                                      << ": record-only flag-index search became a path branch\n";
                            return 1;
                        }
                    }
                }
                if (result.truncated || result.paths.size() >= 100) {
                    std::cerr << argv[i]
                              << ": record-only control flow caused path explosion\n";
                    return 1;
                }
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
    for (const auto& path : symbolic.paths) {
        for (std::size_t i = 0; i < path.events.size(); ++i) {
            const auto& event = path.events[i];
            const std::string expected = event.se_name + ".qasm";
            if (event.qasm_file != expected) {
                std::cerr << "SE event did not retain its declared QASM filename\n";
                return 1;
            }
            const std::string symbolic_id = "id_" + std::to_string(i + 1);
            if (event.syndrome != symbolic_id + ".s" ||
                (event.flag && *event.flag != symbolic_id + ".f")) {
                std::cerr << "SE result did not use sequential id_N symbolic names\n";
                return 1;
            }
            if (event.data_register != "m" ||
                (event.se_name == "first" && event.flag_register != "f") ||
                (event.se_name != "first" && event.flag_register)) {
                std::cerr << "SE event did not retain its QASM output registers\n";
                return 1;
            }
        }
    }
    const auto symbolic_json = symbolic.to_json();
    if (symbolic_json.find("\"qasm_sequence\": [\"first.qasm\"") ==
            std::string::npos ||
        symbolic_json.find("\"qasm_file\": \"left.qasm\"") ==
            std::string::npos ||
        symbolic_json.find("\"qasm_file\": \"right.qasm\"") ==
            std::string::npos ||
        symbolic_json.find("\"data_register\": \"m\"") ==
            std::string::npos ||
        symbolic_json.find("\"flag_register\": \"f\"") ==
            std::string::npos) {
        std::cerr << "symbolic-path JSON did not contain ordered QASM output\n";
        return 1;
    }

    const auto continue_control = fpdl::Parser::parse_string(
        "protocol continue_control: code: [[1,1,1]] [[I]] se: "
        "flagged { file: \"flagged.qasm\" qp: q qm: q cm: m qf: qf cf: f g: [[I]] #-flag: 1 } "
        "plain { file: \"plain.qasm\" qp: q qm: q cm: m g: [[I]] } "
        "gvar: cnt n = 0; bit s = ⊥; bit f = ⊥; bit other = ⊥; "
        "tc: 1: n == 2 "
        "af: while(!tc) { "
        "  (s, f) = flagged(); "
        "  n++; "
        "  if (f != 0) { continue; } "
        "  (other,) = plain(); "
        "} "
        "tp: 1: { end(); }",
        "continue-control.fpdl", symbolic_options);
    bool saw_continue_skip = false;
    bool saw_continue_fallthrough = false;
    for (const auto& path : continue_control.paths) {
        if (path.events.size() < 2) continue;
        if (path.events[0].se_name == "flagged" &&
            path.events[1].se_name == "flagged") {
            saw_continue_skip = true;
        }
        if (path.events[0].se_name == "flagged" &&
            path.events[1].se_name == "plain") {
            saw_continue_fallthrough = true;
        }
    }
    if (!saw_continue_skip || !saw_continue_fallthrough) {
        std::cerr << "continue did not control whether later path-relevant work executes\n";
        return 1;
    }

    const auto compound = fpdl::Parser::parse_string(
        "protocol compound: code: [[1,1,1]] [[I]] se: "
        "flagged { file: \"flagged.qasm\" qp: q qm: q cm: m qf: qf cf: f g: [[I]] #-flag: 1 } "
        "plain { file: \"plain.qasm\" qp: q qm: q cm: m g: [[I]] } "
        "gvar: bit s = ⊥; bit f = ⊥; bit other = ⊥; "
        "tc: 1: (s == 0) and (f == 0) "
        "af: while(!tc) { "
        "  if (s == ⊥) { (s, f) = flagged(); } "
        "  else { (other,) = plain(); } "
        "} "
        "tp: 1: { end(); }",
        "compound.fpdl", symbolic_options);
    if (compound.paths.size() != 2) {
        std::cerr << "one compound false outcome was expanded into multiple control-flow paths\n";
        return 1;
    }
    for (const auto& path : compound.paths) {
        if (path.terminal_condition == 1 && path.rounds > 1) {
            std::cerr << "infeasible !(A and B) then (A and B) path was retained\n";
            return 1;
        }
    }

    return 0;
}
