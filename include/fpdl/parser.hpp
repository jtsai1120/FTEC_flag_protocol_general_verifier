#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fpdl {

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Diagnostic {
    enum class Severity { Warning, Error };

    Severity severity = Severity::Error;
    SourceLocation location;
    std::string message;
};

class ParseError : public std::runtime_error {
public:
    ParseError(SourceLocation location, std::string message);
    [[nodiscard]] SourceLocation location() const noexcept;

private:
    SourceLocation location_;
};

struct SymbolicConstraint {
    std::string expression;
    bool expected = true;
    // Exact execution checkpoint at which this constraint was introduced.
    std::size_t after_event = 0;
    std::size_t round = 0;
    std::string phase;
};

struct SEEvent {
    std::size_t round = 0;
    std::size_t invocation = 0;
    std::string phase;
    std::string se_name;
    // Exact file value from the invoked SE's `file:` declaration.
    std::string qasm_file;
    // QASM classical output registers declared by `cm:` and optional `cf:`.
    std::string data_register;
    std::optional<std::string> flag_register;
    std::string syndrome;
    std::optional<std::string> flag;
};

struct SymbolicPath {
    std::size_t id = 0;
    std::size_t rounds = 0;
    std::size_t transitions = 0;
    std::vector<SEEvent> events;
    std::vector<SymbolicConstraint> constraints;
    std::optional<int> terminal_condition;
    std::optional<int> terminating_policy;
    std::string terminal_action;
    std::optional<std::string> decode_record;
    std::optional<std::string> assertion_error;
    bool terminated = false;
    bool bound_exceeded = false;
};

struct ParseOptions {
    // Symbolic expansion requires a finite transition bound.
    std::optional<std::size_t> bmc_bound;
    std::size_t max_paths = 1000;
    bool inspect_qasm_registers = true;
};

struct ParseResult {
    std::string protocol_name;
    std::vector<SymbolicPath> paths;
    std::vector<Diagnostic> diagnostics;
    bool truncated = false;

    [[nodiscard]] std::string to_json() const;
};

class Parser {
public:
    static ParseResult parse_file(const std::filesystem::path& path,
                                  const ParseOptions& options = {});
    static ParseResult parse_string(std::string_view source,
                                    const std::filesystem::path& source_path,
                                    const ParseOptions& options = {});
};

} // namespace fpdl
