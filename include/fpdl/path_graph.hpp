#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fpdl {

enum class GraphFormat { Svg, Dot, DagJson };

struct GraphOptions {
    // Rendering every symbolic path can create an impractically large image.
    std::size_t max_paths = 100;
    bool show_constraints = true;
};

struct GraphResult {
    std::string protocol_name;
    std::size_t input_path_count = 0;
    std::size_t rendered_path_count = 0;
    bool input_truncated = false;
    bool render_truncated = false;
    std::string content;
};

struct DagGraphOptions {
    bool show_true_conditions = true;
};

class GraphError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] GraphResult render_path_graph_json(
    std::string_view json,
    GraphFormat format,
    const GraphOptions& options = {});

[[nodiscard]] GraphResult render_path_graph_file(
    const std::filesystem::path& path,
    GraphFormat format,
    const GraphOptions& options = {});

// Render the node/edge JSON produced by fpdl-path-dag. Only Svg and Dot are
// valid output formats for these functions.
[[nodiscard]] GraphResult render_dag_graph_json(
    std::string_view json,
    GraphFormat format,
    const DagGraphOptions& options = {});

[[nodiscard]] GraphResult render_dag_graph_file(
    const std::filesystem::path& path,
    GraphFormat format,
    const DagGraphOptions& options = {});

} // namespace fpdl
