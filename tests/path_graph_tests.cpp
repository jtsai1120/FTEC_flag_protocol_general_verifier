#include "fpdl/path_graph.hpp"

#include <iostream>
#include <string>

namespace {

std::size_t occurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

} // namespace

int main() {
    const std::string json = R"json({
      "protocol": "branching",
      "truncated": false,
      "path_count": 2,
      "paths": [
        {
          "id": 0, "rounds": 2, "transitions": 8,
          "terminated": true, "bound_exceeded": false,
          "tc": 1, "tp": 1, "terminal_action": "decode",
          "decode_record": "mr[id_1.s, id_2.s]", "assertion_error": null,
          "events": [
            {"round": 1, "invocation": 1, "phase": "af", "se": "first", "qasm_file": "first.qasm", "s": "id_1.s", "f": "id_1.f"},
            {"round": 2, "invocation": 1, "phase": "tp", "se": "left", "qasm_file": "left.qasm", "s": "id_2.s", "f": null}
          ],
          "constraints": [{"expression": "(id_1.s) == (0)", "expected": true}]
        },
        {
          "id": 1, "rounds": 2, "transitions": 8,
          "terminated": true, "bound_exceeded": false,
          "tc": 2, "tp": 2, "terminal_action": "end",
          "decode_record": null, "assertion_error": null,
          "events": [
            {"round": 1, "invocation": 1, "phase": "af", "se": "first", "qasm_file": "first.qasm", "s": "id_1.s", "f": "id_1.f"},
            {"round": 2, "invocation": 1, "phase": "tp", "se": "right", "qasm_file": "right.qasm", "s": "id_2.s", "f": null}
          ],
          "constraints": [{"expression": "(id_1.s) == (0)", "expected": false}]
        }
      ]
    })json";

    try {
        const auto svg = fpdl::render_path_graph_json(json, fpdl::GraphFormat::Svg);
        if (svg.protocol_name != "branching" || svg.input_path_count != 2 ||
            svg.rendered_path_count != 2 || svg.content.find("<svg") == std::string::npos ||
            svg.content.find("left") == std::string::npos ||
            svg.content.find("right") == std::string::npos ||
            svg.content.find("Path #0") == std::string::npos ||
            occurrences(svg.content, "data-kind=\"event\"") != 3) {
            std::cerr << "SVG graph did not contain the expected shared-prefix tree\n";
            return 1;
        }

        const auto dot = fpdl::render_path_graph_json(json, fpdl::GraphFormat::Dot);
        if (dot.content.find("digraph symbolic_paths") == std::string::npos ||
            dot.content.find("first") == std::string::npos ||
            dot.content.find("NOT ((id_1.s) == (0))") == std::string::npos) {
            std::cerr << "DOT graph did not contain expected nodes\n";
            return 1;
        }

        const auto dag = fpdl::render_path_graph_json(
            json, fpdl::GraphFormat::DagJson);
        if (dag.content.find("\"kind\": \"se\"") == std::string::npos ||
            dag.content.find("\"qasm_file\": \"first.qasm\"") == std::string::npos ||
            dag.content.find("\"condition\": \"(id_1.s) == (0)\"") ==
                std::string::npos ||
            dag.content.find("\"condition\": \"NOT ((id_1.s) == (0))\"") ==
                std::string::npos) {
            std::cerr << "DAG JSON did not contain SE states and control-flow edges\n";
            return 1;
        }

        const auto dag_svg = fpdl::render_dag_graph_json(
            dag.content, fpdl::GraphFormat::Svg);
        if (dag_svg.content.find("<svg") == std::string::npos ||
            dag_svg.content.find("first.qasm") == std::string::npos ||
            dag_svg.content.find("(id_1.s) == (0)") == std::string::npos ||
            occurrences(dag_svg.content, "data-kind=\"se\"") < 3) {
            std::cerr << "DAG SVG did not visualize states and edge conditions\n";
            return 1;
        }

        const auto dag_dot = fpdl::render_dag_graph_json(
            dag.content, fpdl::GraphFormat::Dot);
        if (dag_dot.content.find("digraph protocol_dag") == std::string::npos ||
            dag_dot.content.find("first.qasm") == std::string::npos ||
            dag_dot.content.find("NOT ((id_1.s) == (0))") == std::string::npos) {
            std::cerr << "DAG DOT did not visualize states and edge conditions\n";
            return 1;
        }

        bool cycle_rejected = false;
        try {
            (void)fpdl::render_dag_graph_json(
                R"json({"protocol":"cycle","nodes":[{"id":0,"kind":"start"},{"id":1,"kind":"terminal"}],"edges":[{"from":0,"to":1,"condition":"true"},{"from":1,"to":0,"condition":"true"}]})json",
                fpdl::GraphFormat::Svg);
        } catch (const fpdl::GraphError&) {
            cycle_rejected = true;
        }
        if (!cycle_rejected) {
            std::cerr << "cyclic DAG JSON was accepted by the visualizer\n";
            return 1;
        }

        fpdl::GraphOptions limited;
        limited.max_paths = 1;
        const auto one_path = fpdl::render_path_graph_json(
            json, fpdl::GraphFormat::Svg, limited);
        if (!one_path.render_truncated || one_path.rendered_path_count != 1) {
            std::cerr << "render path limit was not reported\n";
            return 1;
        }

        bool rejected = false;
        try {
            (void)fpdl::render_path_graph_json("{\"protocol\": 1, \"paths\": []}",
                                               fpdl::GraphFormat::Svg);
        } catch (const fpdl::GraphError&) {
            rejected = true;
        }
        if (!rejected) {
            std::cerr << "invalid graph JSON was accepted\n";
            return 1;
        }

        const std::string checkpoint_json = R"json({
          "protocol": "checkpoint", "paths": [
            {"id": 0, "rounds": 2, "transitions": 4, "terminal_action": "end",
             "events": [
               {"round": 1, "invocation": 1, "phase": "af", "se": "first", "s": "$e1", "f": null},
               {"round": 2, "invocation": 1, "phase": "af", "se": "second", "s": "$e2", "f": null}],
             "constraints": [
               {"expression": "round-one-no", "expected": true, "after_event": 1},
               {"expression": "($e1) == (0)", "expected": true, "after_event": 2}]},
            {"id": 1, "rounds": 2, "transitions": 4, "terminal_action": "end",
             "events": [
               {"round": 1, "invocation": 1, "phase": "af", "se": "first", "s": "$e1", "f": null},
               {"round": 2, "invocation": 1, "phase": "af", "se": "second", "s": "$e2", "f": null}],
             "constraints": [
               {"expression": "round-one-no", "expected": true, "after_event": 1},
               {"expression": "($e1) == (0)", "expected": false, "after_event": 2}]}
          ]
        })json";
        const auto checkpoint = fpdl::render_path_graph_json(
            checkpoint_json, fpdl::GraphFormat::Dot);
        if (checkpoint.content.find("n2 -> n3;") == std::string::npos ||
            checkpoint.content.find("n3 -> n4;") == std::string::npos ||
            checkpoint.content.find("n3 -> n6;") == std::string::npos) {
            std::cerr << "constraint checkpoint metadata was not used for graph placement\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
