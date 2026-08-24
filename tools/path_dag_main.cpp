#include "fpdl/path_graph.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

void usage(const char* argv0) {
    std::cerr << "usage: " << argv0
              << " <paths.json> [-o dag.json] [--max-paths N]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    std::filesystem::path input;
    std::optional<std::filesystem::path> output;
    fpdl::GraphOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o") {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            output = argv[i];
        } else if (arg == "--max-paths") {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            try {
                const auto parsed = std::stoull(argv[i]);
                if (parsed == 0) throw std::invalid_argument("zero");
                options.max_paths = parsed;
            } catch (const std::exception&) {
                std::cerr << "error: --max-paths must be a positive integer\n";
                return 2;
            }
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

    try {
        const auto result = fpdl::render_path_graph_file(
            input, fpdl::GraphFormat::DagJson, options);
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
        if (result.render_truncated) {
            std::cerr << "warning: included " << result.rendered_path_count << " of "
                      << result.input_path_count
                      << " paths; raise --max-paths to include more\n";
        }
        if (result.input_truncated) {
            std::cerr << "warning: input JSON was already truncated by the parser\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
