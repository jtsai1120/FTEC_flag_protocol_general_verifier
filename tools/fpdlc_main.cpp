#include "fpdl/parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

void usage(const char* argv0) {
    std::cerr << "usage: " << argv0
              << " <input.fpdl> [-o paths.json] --bmc-bound N [--max-paths N]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    std::filesystem::path input;
    std::optional<std::filesystem::path> output;
    fpdl::ParseOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o") {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            output = argv[i];
        } else if (arg == "--bmc-bound") {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            try {
                const auto parsed = std::stoull(argv[i]);
                if (parsed == 0) throw std::invalid_argument("zero");
                options.bmc_bound = parsed;
            } catch (const std::exception&) {
                std::cerr << "error: --bmc-bound must be a positive integer\n";
                return 2;
            }
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
    if (!options.bmc_bound) {
        std::cerr << "error: --bmc-bound is required\n";
        return 2;
    }

    try {
        auto result = fpdl::Parser::parse_file(input, options);
        const std::string json = result.to_json();

        if (output) {
            std::ofstream stream(*output);
            if (!stream) {
                std::cerr << "error: cannot open output file " << *output << '\n';
                return 1;
            }
            stream << json << '\n';
        } else {
            std::cout << json << '\n';
        }

        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << input.string() << ':' << diagnostic.location.line << ':'
                      << diagnostic.location.column << ": warning: "
                      << diagnostic.message << '\n';
        }
        return 0;
    } catch (const fpdl::ParseError& error) {
        std::cerr << input.string() << ':' << error.location().line << ':'
                  << error.location().column << ": error: " << error.what() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
    }
    return 1;
}
