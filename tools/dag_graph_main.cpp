#include "fpdl/path_graph.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

void usage(const char* argv0) {
    std::cerr << "usage: " << argv0
              << " <dag.json> [-o graph.svg|graph.dot] [--format svg|dot]"
                 " [--hide-true-conditions]\n";
}

std::optional<fpdl::GraphFormat> parse_format(const std::string& value) {
    if (value == "svg") return fpdl::GraphFormat::Svg;
    if (value == "dot") return fpdl::GraphFormat::Dot;
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    std::filesystem::path input;
    std::optional<std::filesystem::path> output;
    std::optional<fpdl::GraphFormat> requested_format;
    fpdl::DagGraphOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o") {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            output = argv[i];
        } else if (arg == "--format") {
            if (++i >= argc || !(requested_format = parse_format(argv[i]))) {
                std::cerr << "error: --format must be svg or dot\n";
                return 2;
            }
        } else if (arg == "--hide-true-conditions") {
            options.show_true_conditions = false;
        } else if (!arg.empty() && arg.front() == '-') {
            std::cerr << "error: unknown option " << arg << '\n';
            return 2;
        } else if (input.empty()) {
            input = arg;
        } else {
            std::cerr << "error: multiple input files\n";
            return 2;
        }
    }

    if (input.empty()) {
        usage(argv[0]);
        return 2;
    }

    fpdl::GraphFormat format = requested_format.value_or(fpdl::GraphFormat::Svg);
    if (!requested_format && output) {
        if (output->extension() == ".dot" || output->extension() == ".gv") {
            format = fpdl::GraphFormat::Dot;
        } else if (output->extension() != ".svg") {
            std::cerr << "error: output extension must be .svg, .dot, or .gv; "
                         "use --format to override\n";
            return 2;
        }
    }

    try {
        const auto result = fpdl::render_dag_graph_file(input, format, options);
        if (output) {
            std::ofstream stream(*output, std::ios::binary);
            if (!stream) {
                std::cerr << "error: cannot open output file " << *output << '\n';
                return 1;
            }
            stream << result.content;
            if (!stream) {
                std::cerr << "error: failed while writing output file " << *output << '\n';
                return 1;
            }
        } else {
            std::cout << result.content;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
