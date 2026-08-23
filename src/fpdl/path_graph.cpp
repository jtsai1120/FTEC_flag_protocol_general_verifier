#include "fpdl/path_graph.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fpdl {
namespace {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    explicit Json(Value value) : value_(std::move(value)) {}

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(value_); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(value_); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<Array>(value_); }
    [[nodiscard]] bool is_object() const { return std::holds_alternative<Object>(value_); }

    [[nodiscard]] bool boolean() const { return std::get<bool>(value_); }
    [[nodiscard]] double number() const { return std::get<double>(value_); }
    [[nodiscard]] const std::string& string() const { return std::get<std::string>(value_); }
    [[nodiscard]] const Array& array() const { return std::get<Array>(value_); }
    [[nodiscard]] const Object& object() const { return std::get<Object>(value_); }

private:
    Value value_;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    [[nodiscard]] Json parse() {
        skip_space();
        Json result = parse_value();
        skip_space();
        if (position_ != input_.size()) fail("unexpected trailing input");
        return result;
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < position_ && i < input_.size(); ++i) {
            if (input_[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        throw GraphError("JSON " + std::to_string(line) + ":" +
                         std::to_string(column) + ": " + message);
    }

    void skip_space() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    [[nodiscard]] bool consume(char value) {
        if (position_ < input_.size() && input_[position_] == value) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char value) {
        if (!consume(value)) fail(std::string("expected '") + value + "'");
    }

    [[nodiscard]] Json parse_value() {
        if (position_ >= input_.size()) fail("expected a value");
        switch (input_[position_]) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': return Json(parse_string());
        case 't': parse_literal("true"); return Json(true);
        case 'f': parse_literal("false"); return Json(false);
        case 'n': parse_literal("null"); return Json(nullptr);
        default:
            if (input_[position_] == '-' ||
                std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return Json(parse_number());
            }
            fail("expected a JSON value");
        }
    }

    void parse_literal(std::string_view literal) {
        if (input_.substr(position_, literal.size()) != literal) {
            fail("invalid literal");
        }
        position_ += literal.size();
    }

    [[nodiscard]] Json parse_object() {
        expect('{');
        skip_space();
        Json::Object object;
        if (consume('}')) return Json(std::move(object));
        while (true) {
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("expected an object key");
            }
            std::string key = parse_string();
            skip_space();
            expect(':');
            skip_space();
            auto [unused, inserted] = object.emplace(std::move(key), parse_value());
            (void)unused;
            if (!inserted) fail("duplicate object key");
            skip_space();
            if (consume('}')) break;
            expect(',');
            skip_space();
        }
        return Json(std::move(object));
    }

    [[nodiscard]] Json parse_array() {
        expect('[');
        skip_space();
        Json::Array array;
        if (consume(']')) return Json(std::move(array));
        while (true) {
            array.push_back(parse_value());
            skip_space();
            if (consume(']')) break;
            expect(',');
            skip_space();
        }
        return Json(std::move(array));
    }

    static void append_utf8(std::string& output, unsigned codepoint) {
        if (codepoint <= 0x7f) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    [[nodiscard]] unsigned parse_hex4() {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            if (position_ >= input_.size()) fail("incomplete Unicode escape");
            const char c = input_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid Unicode escape");
        }
        return value;
    }

    [[nodiscard]] std::string parse_string() {
        expect('"');
        std::string output;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') return output;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                output.push_back(c);
                continue;
            }
            if (position_ >= input_.size()) fail("incomplete string escape");
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                unsigned codepoint = parse_hex4();
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (position_ + 2 > input_.size() || input_[position_] != '\\' ||
                        input_[position_ + 1] != 'u') {
                        fail("unpaired high surrogate");
                    }
                    position_ += 2;
                    const unsigned low = parse_hex4();
                    if (low < 0xdc00 || low > 0xdfff) fail("invalid low surrogate");
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
                                (low - 0xdc00);
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    fail("unpaired low surrogate");
                }
                append_utf8(output, codepoint);
                break;
            }
            default: fail("invalid string escape");
            }
        }
        fail("unterminated string");
    }

    [[nodiscard]] double parse_number() {
        const std::size_t start = position_;
        (void)consume('-');
        if (consume('0')) {
            if (position_ < input_.size() &&
                std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("leading zero in number");
            }
        } else {
            if (position_ >= input_.size() ||
                !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("invalid number");
            }
            while (position_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        if (consume('.')) {
            if (position_ >= input_.size() ||
                !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("invalid fractional part");
            }
            while (position_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        if (position_ < input_.size() &&
            (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() &&
                (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            if (position_ >= input_.size() ||
                !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                fail("invalid exponent");
            }
            while (position_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        try {
            const double value = std::stod(std::string(input_.substr(start, position_ - start)));
            if (!std::isfinite(value)) fail("number is outside the supported range");
            return value;
        } catch (const std::exception&) {
            fail("invalid number");
        }
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

const Json& require_member(const Json::Object& object, const std::string& key,
                           const std::string& context) {
    const auto found = object.find(key);
    if (found == object.end()) throw GraphError(context + ": missing '" + key + "'");
    return found->second;
}

const Json* optional_member(const Json::Object& object, const std::string& key) {
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
}

const Json::Object& require_object(const Json& value, const std::string& context) {
    if (!value.is_object()) throw GraphError(context + ": expected an object");
    return value.object();
}

const Json::Array& require_array(const Json& value, const std::string& context) {
    if (!value.is_array()) throw GraphError(context + ": expected an array");
    return value.array();
}

std::string require_string(const Json& value, const std::string& context) {
    if (!value.is_string()) throw GraphError(context + ": expected a string");
    return value.string();
}

std::string optional_string(const Json::Object& object, const std::string& key,
                            const std::string& fallback = {}) {
    const Json* value = optional_member(object, key);
    if (value == nullptr || value->is_null()) return fallback;
    return require_string(*value, key);
}

bool optional_bool(const Json::Object& object, const std::string& key, bool fallback) {
    const Json* value = optional_member(object, key);
    if (value == nullptr) return fallback;
    if (!value->is_bool()) throw GraphError(key + ": expected a Boolean");
    return value->boolean();
}

std::size_t number_to_size(const Json& value, const std::string& context) {
    if (!value.is_number()) throw GraphError(context + ": expected a number");
    const double number = value.number();
    if (number < 0 || std::floor(number) != number ||
        number > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw GraphError(context + ": expected a non-negative integer");
    }
    return static_cast<std::size_t>(number);
}

std::optional<std::size_t> optional_size(const Json::Object& object,
                                         const std::string& key) {
    const Json* value = optional_member(object, key);
    if (value == nullptr || value->is_null()) return std::nullopt;
    return number_to_size(*value, key);
}

std::string xml_escape(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (const char c : input) {
        switch (c) {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&apos;"; break;
        default: output.push_back(c); break;
        }
    }
    return output;
}

std::string dot_escape(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (const char c : input) {
        switch (c) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\n': output += "\\n"; break;
        case '\r': break;
        default: output.push_back(c); break;
        }
    }
    return output;
}

std::string short_text(std::string text, std::size_t limit) {
    if (text.size() <= limit) return text;
    if (limit <= 3) return text.substr(0, limit);
    return text.substr(0, limit - 3) + "...";
}

std::vector<std::string> wrap_text(std::string_view input, std::size_t width,
                                   std::size_t max_lines) {
    std::vector<std::string> lines;
    std::istringstream blocks{std::string(input)};
    std::string block;
    while (std::getline(blocks, block, '\n')) {
        if (block.empty()) {
            lines.emplace_back();
            continue;
        }
        std::istringstream words(block);
        std::string word;
        std::string line;
        while (words >> word) {
            if (line.empty()) {
                line = std::move(word);
            } else if (line.size() + 1 + word.size() <= width) {
                line += ' ';
                line += word;
            } else {
                lines.push_back(std::move(line));
                line = std::move(word);
                if (lines.size() == max_lines) break;
            }
        }
        if (lines.size() < max_lines && !line.empty()) lines.push_back(std::move(line));
        if (lines.size() >= max_lines) break;
    }
    if (lines.empty()) lines.emplace_back();
    if (lines.size() == max_lines) {
        lines.back() = short_text(std::move(lines.back()), width);
    }
    return lines;
}

enum class NodeKind { Start, Event, Constraint, Terminal, Error, Bound };

struct Node {
    std::size_t id = 0;
    NodeKind kind = NodeKind::Start;
    std::size_t depth = 0;
    std::string label;
    std::string title;
    std::map<std::string, std::size_t> children;
    std::size_t round = 0;
    std::size_t invocation = 0;
    std::string phase;
    std::string se;
    std::string qasm_file;
    std::string data_register;
    std::string flag_register;
    std::optional<std::size_t> path_id;
    double x = 0;
    double y = 0;
};

struct EventData {
    std::size_t round = 0;
    std::size_t invocation = 0;
    std::string phase;
    std::string se;
    std::string qasm_file;
    std::string data_register;
    std::string flag_register;
    std::string syndrome;
    std::string flag;
};

struct ConstraintData {
    std::string expression;
    bool expected = true;
    std::optional<std::size_t> after_event;
};

struct GraphModel {
    std::string protocol;
    std::vector<Node> nodes;
    std::size_t input_paths = 0;
    std::size_t rendered_paths = 0;
    bool input_truncated = false;
    bool render_truncated = false;
};

std::string join_constraints(const std::vector<ConstraintData>& constraints,
                             bool compact) {
    std::string output;
    for (std::size_t i = 0; i < constraints.size(); ++i) {
        if (i != 0) output += compact ? " AND " : "\nAND ";
        if (!constraints[i].expected) {
            output += "NOT (" + constraints[i].expression + ")";
        } else {
            output += constraints[i].expression;
        }
    }
    return output;
}

std::size_t add_child(GraphModel& graph, std::size_t parent, NodeKind kind,
                      const std::string& key, std::string label, std::string title) {
    if (const auto found = graph.nodes[parent].children.find(key);
        found != graph.nodes[parent].children.end()) {
        return found->second;
    }
    const std::size_t id = graph.nodes.size();
    const std::size_t depth = graph.nodes[parent].depth + 1;
    Node node;
    node.id = id;
    node.kind = kind;
    node.depth = depth;
    node.label = std::move(label);
    node.title = std::move(title);
    graph.nodes.push_back(std::move(node));
    graph.nodes[parent].children.emplace(key, id);
    return id;
}

GraphModel build_graph(const Json& root_value, const GraphOptions& options) {
    if (options.max_paths == 0) throw GraphError("max_paths must be positive");
    const auto& root = require_object(root_value, "root");
    GraphModel graph;
    graph.protocol = require_string(require_member(root, "protocol", "root"), "protocol");
    graph.input_truncated = optional_bool(root, "truncated", false);
    const auto& paths = require_array(require_member(root, "paths", "root"), "paths");
    graph.input_paths = paths.size();
    graph.rendered_paths = std::min(paths.size(), options.max_paths);
    graph.render_truncated = graph.rendered_paths < graph.input_paths;
    Node start;
    start.label = "START\n" + graph.protocol;
    start.title = "Protocol " + graph.protocol;
    graph.nodes.push_back(std::move(start));

    for (std::size_t path_index = 0; path_index < graph.rendered_paths; ++path_index) {
        const std::string context = "paths[" + std::to_string(path_index) + "]";
        const auto& path = require_object(paths[path_index], context);
        const std::size_t path_id = optional_size(path, "id").value_or(path_index);
        const auto& event_values = require_array(
            require_member(path, "events", context), context + ".events");
        std::vector<EventData> events;
        events.reserve(event_values.size());
        for (std::size_t i = 0; i < event_values.size(); ++i) {
            const auto& event = require_object(event_values[i], context + ".events[" +
                                                std::to_string(i) + "]");
            EventData data;
            data.round = optional_size(event, "round").value_or(0);
            data.invocation = optional_size(event, "invocation").value_or(0);
            data.phase = optional_string(event, "phase", "?");
            data.se = require_string(require_member(event, "se", "event"), "event.se");
            data.qasm_file = optional_string(event, "qasm_file");
            data.data_register = optional_string(event, "data_register");
            data.flag_register = optional_string(event, "flag_register");
            data.syndrome = optional_string(event, "s", "?");
            data.flag = optional_string(event, "f");
            events.push_back(std::move(data));
        }

        std::vector<ConstraintData> constraints;
        if (const Json* value = optional_member(path, "constraints")) {
            const auto& values = require_array(*value, context + ".constraints");
            constraints.reserve(values.size());
            for (std::size_t i = 0; i < values.size(); ++i) {
                const auto& constraint = require_object(
                    values[i], context + ".constraints[" + std::to_string(i) + "]");
                constraints.push_back(ConstraintData{
                    require_string(require_member(constraint, "expression", "constraint"),
                                   "constraint.expression"),
                    optional_bool(constraint, "expected", true),
                    optional_size(constraint, "after_event")});
            }
        }

        std::vector<std::vector<ConstraintData>> buckets(events.size() + 1);
        for (const auto& constraint : constraints) {
            if (constraint.after_event) {
                buckets[std::min(*constraint.after_event, events.size())].push_back(constraint);
                continue;
            }

            // Backward compatibility for JSON produced before checkpoint
            // metadata was added: infer a best-effort location from the latest
            // symbolic SE value mentioned by the expression.
            std::optional<std::size_t> last_event;
            for (std::size_t i = 0; i < events.size(); ++i) {
                const bool mentions_s = !events[i].syndrome.empty() &&
                                        constraint.expression.find(events[i].syndrome) !=
                                            std::string::npos;
                const bool mentions_f = !events[i].flag.empty() &&
                                        constraint.expression.find(events[i].flag) !=
                                            std::string::npos;
                if (mentions_s || mentions_f) last_event = i;
            }
            buckets[last_event ? *last_event + 1 : events.size()].push_back(constraint);
        }

        std::size_t current = 0;
        auto append_constraint = [&](std::size_t bucket) {
            if (!options.show_constraints || buckets[bucket].empty()) return;
            const std::string full = join_constraints(buckets[bucket], false);
            const std::string condition_key =
                "condition|" + join_constraints(buckets[bucket], true);
            current = add_child(graph, current, NodeKind::Constraint, condition_key,
                                "BRANCH\n" + full, full);
        };

        append_constraint(0);
        for (std::size_t i = 0; i < events.size(); ++i) {
            const auto& event = events[i];
            std::ostringstream key;
            key << "event|" << event.round << '|' << event.invocation << '|'
                << event.phase << '|' << event.se << '|' << event.qasm_file << '|'
                << event.data_register << '|' << event.flag_register << '|'
                << event.syndrome << '|' << event.flag;
            std::ostringstream label;
            label << "Round " << event.round << " · " << event.phase << " #"
                  << event.invocation << '\n' << event.se << '\n'
                  << "s = " << event.syndrome;
            if (!event.flag.empty()) label << '\n' << "f = " << event.flag;
            current = add_child(graph, current, NodeKind::Event, key.str(), label.str(),
                                label.str());
            auto& event_node = graph.nodes[current];
            event_node.round = event.round;
            event_node.invocation = event.invocation;
            event_node.phase = event.phase;
            event_node.se = event.se;
            event_node.qasm_file = event.qasm_file;
            event_node.data_register = event.data_register;
            event_node.flag_register = event.flag_register;
            append_constraint(i + 1);
        }

        const bool bound = optional_bool(path, "bound_exceeded", false);
        const std::string assertion = optional_string(path, "assertion_error");
        const std::string action = optional_string(path, "terminal_action", "unterminated");
        const auto tc = optional_size(path, "tc");
        const auto tp = optional_size(path, "tp");
        const auto rounds = optional_size(path, "rounds").value_or(0);
        const auto transitions = optional_size(path, "transitions").value_or(0);
        const std::string record = optional_string(path, "decode_record");

        NodeKind terminal_kind = NodeKind::Terminal;
        if (!assertion.empty()) terminal_kind = NodeKind::Error;
        else if (bound) terminal_kind = NodeKind::Bound;

        std::ostringstream label;
        label << "Path #" << path_id << '\n';
        if (!assertion.empty()) label << "ASSERTION ERROR\n" << assertion;
        else if (bound) label << "BMC BOUND EXCEEDED";
        else {
            label << action;
            if (tc) label << " · tc " << *tc;
            if (tp) label << " / tp " << *tp;
            label << '\n' << rounds << " rounds · " << transitions << " transitions";
        }
        std::string title = label.str();
        if (!record.empty()) title += "\ndecode record: " + record;
        const std::string terminal_key = "terminal|" + std::to_string(path_id);
        const auto terminal = add_child(graph, current, terminal_kind, terminal_key,
                                        label.str(), title);
        graph.nodes[terminal].path_id = path_id;
    }
    return graph;
}

double assign_x(GraphModel& graph, std::size_t id, std::size_t& leaf_index,
                double margin, double leaf_gap) {
    auto& node = graph.nodes[id];
    if (node.children.empty()) {
        node.x = margin + static_cast<double>(leaf_index++) * leaf_gap;
        return node.x;
    }
    double total = 0;
    for (const auto& [unused, child] : node.children) {
        (void)unused;
        total += assign_x(graph, child, leaf_index, margin, leaf_gap);
    }
    node.x = total / static_cast<double>(node.children.size());
    return node.x;
}

std::string kind_name(NodeKind kind) {
    switch (kind) {
    case NodeKind::Start: return "start";
    case NodeKind::Event: return "event";
    case NodeKind::Constraint: return "constraint";
    case NodeKind::Terminal: return "terminal";
    case NodeKind::Error: return "error";
    case NodeKind::Bound: return "bound";
    }
    return "unknown";
}

std::pair<double, double> node_dimensions(NodeKind kind) {
    switch (kind) {
    case NodeKind::Start: return {280, 76};
    case NodeKind::Event: return {300, 112};
    case NodeKind::Constraint: return {330, 126};
    case NodeKind::Terminal:
    case NodeKind::Error:
    case NodeKind::Bound: return {300, 104};
    }
    return {300, 100};
}

std::string render_svg(GraphModel graph) {
    constexpr double margin_x = 210;
    constexpr double header_height = 135;
    constexpr double leaf_gap = 380;
    constexpr double depth_gap = 185;
    std::size_t leaf_index = 0;
    assign_x(graph, 0, leaf_index, margin_x, leaf_gap);
    std::size_t max_depth = 0;
    for (auto& node : graph.nodes) {
        max_depth = std::max(max_depth, node.depth);
        node.y = header_height + static_cast<double>(node.depth) * depth_gap;
    }
    const double width = std::max(760.0, margin_x * 2 +
        static_cast<double>(leaf_index > 0 ? leaf_index - 1 : 0) * leaf_gap);
    const double height = header_height + static_cast<double>(max_depth) * depth_gap + 120;

    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
        << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << ' '
        << height << "\" role=\"img\" aria-labelledby=\"graph-title graph-desc\">\n"
        << "<title id=\"graph-title\">Symbolic paths for "
        << xml_escape(graph.protocol) << "</title>\n"
        << "<desc id=\"graph-desc\">A common-prefix tree of symbolic SE events, "
           "branch constraints, and terminating policies.</desc>\n"
        << "<defs><marker id=\"arrow\" markerWidth=\"8\" markerHeight=\"8\" "
           "refX=\"7\" refY=\"4\" orient=\"auto\" markerUnits=\"strokeWidth\">"
           "<path d=\"M0,0 L8,4 L0,8 z\" fill=\"#8290a7\"/></marker>"
           "<filter id=\"shadow\" x=\"-15%\" y=\"-15%\" width=\"130%\" height=\"140%\">"
           "<feDropShadow dx=\"0\" dy=\"3\" stdDeviation=\"3\" flood-opacity=\"0.12\"/>"
           "</filter></defs>\n"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#f6f8fc\"/>\n"
        << "<text x=\"40\" y=\"42\" font-family=\"ui-sans-serif, system-ui, sans-serif\" "
           "font-size=\"24\" font-weight=\"700\" fill=\"#172033\">"
        << xml_escape(graph.protocol) << "</text>\n"
        << "<text x=\"40\" y=\"70\" font-family=\"ui-sans-serif, system-ui, sans-serif\" "
           "font-size=\"14\" fill=\"#667085\">"
        << graph.rendered_paths << " of " << graph.input_paths << " symbolic paths";
    if (graph.input_truncated) out << " · parser output truncated";
    if (graph.render_truncated) out << " · render limit reached";
    out << "</text>\n";

    for (const auto& parent : graph.nodes) {
        const auto [parent_width, parent_height] = node_dimensions(parent.kind);
        for (const auto& [unused, child_id] : parent.children) {
            (void)unused;
            const auto& child = graph.nodes[child_id];
            const auto [child_width, child_height] = node_dimensions(child.kind);
            (void)parent_width;
            (void)child_width;
            const double from_y = parent.y + parent_height / 2;
            const double to_y = child.y - child_height / 2 - 5;
            const double middle_y = (from_y + to_y) / 2;
            out << "<path d=\"M " << parent.x << ' ' << from_y << " C "
                << parent.x << ' ' << middle_y << ", " << child.x << ' '
                << middle_y << ", " << child.x << ' ' << to_y
                << "\" fill=\"none\" stroke=\"#8290a7\" stroke-width=\"2\" "
                   "marker-end=\"url(#arrow)\"/>\n";
        }
    }

    for (const auto& node : graph.nodes) {
        const auto [node_width, node_height] = node_dimensions(node.kind);
        const double left = node.x - node_width / 2;
        const double top = node.y - node_height / 2;
        std::string fill;
        std::string stroke;
        switch (node.kind) {
        case NodeKind::Start: fill = "#172033"; stroke = "#172033"; break;
        case NodeKind::Event: fill = "#e7f0ff"; stroke = "#3973cf"; break;
        case NodeKind::Constraint: fill = "#fff3d6"; stroke = "#c47b14"; break;
        case NodeKind::Terminal: fill = "#e5f7ed"; stroke = "#258a55"; break;
        case NodeKind::Error: fill = "#fde8e7"; stroke = "#c43d36"; break;
        case NodeKind::Bound: fill = "#fff0df"; stroke = "#d16718"; break;
        }
        out << "<g id=\"node-" << node.id << "\" data-kind=\""
            << kind_name(node.kind) << "\"><title>" << xml_escape(node.title)
            << "</title>\n";
        if (node.kind == NodeKind::Constraint) {
            out << "<polygon points=\"" << node.x << ',' << top << ' '
                << left + node_width << ',' << node.y << ' ' << node.x << ','
                << top + node_height << ' ' << left << ',' << node.y
                << "\" fill=\"" << fill << "\" stroke=\"" << stroke
                << "\" stroke-width=\"2\" filter=\"url(#shadow)\"/>\n";
        } else {
            out << "<rect x=\"" << left << "\" y=\"" << top << "\" width=\""
                << node_width << "\" height=\"" << node_height
                << "\" rx=\"" << (node.kind == NodeKind::Start ? 28 : 12)
                << "\" fill=\"" << fill << "\" stroke=\"" << stroke
                << "\" stroke-width=\"2\" filter=\"url(#shadow)\"/>\n";
        }

        const auto lines = wrap_text(node.label,
                                     node.kind == NodeKind::Constraint ? 38 : 42,
                                     node.kind == NodeKind::Constraint ? 5 : 5);
        const double line_height = 18;
        const double first_y = node.y - (static_cast<double>(lines.size() - 1) *
                                          line_height / 2) + 5;
        const std::string text_fill = node.kind == NodeKind::Start ? "#ffffff" : "#172033";
        out << "<text x=\"" << node.x << "\" y=\"" << first_y
            << "\" text-anchor=\"middle\" font-family=\"ui-monospace, SFMono-Regular, "
               "Menlo, Consolas, monospace\" font-size=\"13\" fill=\""
            << text_fill << "\">";
        for (std::size_t i = 0; i < lines.size(); ++i) {
            out << "<tspan x=\"" << node.x << "\" dy=\""
                << (i == 0 ? 0 : line_height) << "\""
                << (i == 0 ? " font-weight=\"700\"" : "") << '>'
                << xml_escape(lines[i]) << "</tspan>";
        }
        out << "</text></g>\n";
    }
    out << "</svg>\n";
    return out.str();
}

std::string render_dot(const GraphModel& graph) {
    std::ostringstream out;
    out << "digraph symbolic_paths {\n"
        << "  graph [rankdir=TB, bgcolor=\"#f6f8fc\", pad=0.4, nodesep=0.35, ranksep=0.65, "
           "labelloc=t, label=\"" << dot_escape(graph.protocol) << "\\n"
        << graph.rendered_paths << " of " << graph.input_paths << " symbolic paths\"];\n"
        << "  node [shape=box, style=\"rounded,filled\", fontname=\"Menlo\", fontsize=10, "
           "color=\"#3973cf\", fillcolor=\"#e7f0ff\"];\n"
        << "  edge [color=\"#8290a7\", arrowsize=0.7];\n";
    for (const auto& node : graph.nodes) {
        std::string attributes;
        switch (node.kind) {
        case NodeKind::Start:
            attributes = "shape=oval, color=\"#172033\", fillcolor=\"#172033\", fontcolor=white";
            break;
        case NodeKind::Event: break;
        case NodeKind::Constraint:
            attributes = "shape=diamond, color=\"#c47b14\", fillcolor=\"#fff3d6\"";
            break;
        case NodeKind::Terminal:
            attributes = "color=\"#258a55\", fillcolor=\"#e5f7ed\"";
            break;
        case NodeKind::Error:
            attributes = "color=\"#c43d36\", fillcolor=\"#fde8e7\"";
            break;
        case NodeKind::Bound:
            attributes = "color=\"#d16718\", fillcolor=\"#fff0df\"";
            break;
        }
        out << "  n" << node.id << " [label=\"" << dot_escape(node.label) << '"';
        if (!attributes.empty()) out << ", " << attributes;
        out << "];\n";
    }
    for (const auto& node : graph.nodes) {
        for (const auto& [unused, child] : node.children) {
            (void)unused;
            out << "  n" << node.id << " -> n" << child << ";\n";
        }
    }
    out << "}\n";
    return out.str();
}

std::string json_escape(std::string_view input) {
    std::ostringstream output;
    for (const unsigned char c : input) {
        switch (c) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (c < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned>(c) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(c);
            }
        }
    }
    return output.str();
}

std::string render_dag_json(const GraphModel& graph) {
    const auto omitted = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> dag_ids(graph.nodes.size(), omitted);
    std::size_t node_count = 0;
    for (const auto& node : graph.nodes) {
        if (node.kind != NodeKind::Constraint) dag_ids[node.id] = node_count++;
    }

    std::ostringstream out;
    out << "{\n  \"protocol\": \"" << json_escape(graph.protocol)
        << "\",\n  \"input_path_count\": " << graph.input_paths
        << ",\n  \"included_path_count\": " << graph.rendered_paths
        << ",\n  \"input_truncated\": "
        << (graph.input_truncated ? "true" : "false")
        << ",\n  \"dag_truncated\": "
        << (graph.render_truncated ? "true" : "false")
        << ",\n  \"nodes\": [\n";

    bool first_node = true;
    for (const auto& node : graph.nodes) {
        if (node.kind == NodeKind::Constraint) continue;
        if (!first_node) out << ",\n";
        first_node = false;
        const std::string kind = node.kind == NodeKind::Event ? "se" : kind_name(node.kind);
        out << "    {\"id\": " << dag_ids[node.id]
            << ", \"kind\": \"" << kind << '"';
        if (node.kind == NodeKind::Event) {
            out << ", \"round\": " << node.round
                << ", \"invocation\": " << node.invocation
                << ", \"phase\": \"" << json_escape(node.phase)
                << "\", \"se\": \"" << json_escape(node.se)
                << "\", \"qasm_file\": ";
            if (node.qasm_file.empty()) out << "null";
            else out << '"' << json_escape(node.qasm_file) << '"';
            out << ", \"data_register\": ";
            if (node.data_register.empty()) out << "null";
            else out << '"' << json_escape(node.data_register) << '"';
            out << ", \"flag_register\": ";
            if (node.flag_register.empty()) out << "null";
            else out << '"' << json_escape(node.flag_register) << '"';
        } else if (node.path_id) {
            out << ", \"path_id\": " << *node.path_id
                << ", \"summary\": \"" << json_escape(node.title) << '"';
        }
        out << '}';
    }
    out << "\n  ],\n  \"edges\": [\n";

    bool first_edge = true;
    auto append_edge = [&](std::size_t from, std::size_t to,
                           std::string_view condition) {
        if (!first_edge) out << ",\n";
        first_edge = false;
        out << "    {\"from\": " << dag_ids[from]
            << ", \"to\": " << dag_ids[to]
            << ", \"condition\": \"" << json_escape(condition) << "\"}";
    };

    for (const auto& parent : graph.nodes) {
        if (parent.kind == NodeKind::Constraint) continue;
        for (const auto& [unused, child_id] : parent.children) {
            (void)unused;
            const auto& child = graph.nodes[child_id];
            if (child.kind != NodeKind::Constraint) {
                append_edge(parent.id, child.id, "true");
                continue;
            }
            for (const auto& [constraint_key, target_id] : child.children) {
                (void)constraint_key;
                if (graph.nodes[target_id].kind == NodeKind::Constraint) {
                    throw GraphError("internal DAG conversion found nested constraint nodes");
                }
                append_edge(parent.id, target_id, child.title);
            }
        }
    }
    out << "\n  ]\n}\n";
    return out.str();
}

struct DagVisualNode {
    std::size_t source_id = 0;
    std::string kind;
    std::string label;
    std::string title;
    std::size_t depth = 0;
    double x = 0;
    double y = 0;
};

struct DagVisualEdge {
    std::size_t from = 0;
    std::size_t to = 0;
    std::string condition;
};

struct DagVisualModel {
    std::string protocol;
    std::vector<DagVisualNode> nodes;
    std::vector<DagVisualEdge> edges;
    std::size_t input_paths = 0;
    std::size_t included_paths = 0;
    bool input_truncated = false;
    bool dag_truncated = false;
};

DagVisualModel parse_dag_model(const Json& root_value) {
    const auto& root = require_object(root_value, "root");
    DagVisualModel model;
    model.protocol = require_string(require_member(root, "protocol", "root"),
                                    "protocol");
    model.input_paths = optional_size(root, "input_path_count").value_or(0);
    model.included_paths = optional_size(root, "included_path_count")
                               .value_or(model.input_paths);
    model.input_truncated = optional_bool(root, "input_truncated", false);
    model.dag_truncated = optional_bool(root, "dag_truncated", false);

    const auto& nodes = require_array(require_member(root, "nodes", "root"),
                                      "nodes");
    if (nodes.empty()) throw GraphError("DAG must contain at least one node");
    std::map<std::size_t, std::size_t> index_by_id;
    model.nodes.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const std::string context = "nodes[" + std::to_string(i) + "]";
        const auto& object = require_object(nodes[i], context);
        const auto id = number_to_size(require_member(object, "id", context),
                                       context + ".id");
        if (!index_by_id.emplace(id, model.nodes.size()).second) {
            throw GraphError(context + ": duplicate node id " + std::to_string(id));
        }
        DagVisualNode node;
        node.source_id = id;
        node.kind = require_string(require_member(object, "kind", context),
                                   context + ".kind");
        if (node.kind == "start") {
            node.label = "START\n" + model.protocol;
            node.title = "Protocol " + model.protocol;
        } else if (node.kind == "se") {
            const std::string se = optional_string(object, "se", "?");
            const std::string qasm = optional_string(object, "qasm_file", "?");
            const std::string data = optional_string(object, "data_register", "?");
            const std::string flag = optional_string(object, "flag_register", "none");
            const std::string phase = optional_string(object, "phase", "?");
            const auto round = optional_size(object, "round").value_or(0);
            const auto invocation = optional_size(object, "invocation").value_or(0);
            std::ostringstream label;
            label << "State " << id << " · " << se << '\n'
                  << "QASM: " << qasm << '\n'
                  << "data: " << data << " · flag: " << flag << '\n'
                  << "Round " << round << " · " << phase << " #" << invocation;
            node.label = label.str();
            node.title = node.label;
        } else if (node.kind == "terminal" || node.kind == "error" ||
                   node.kind == "bound") {
            node.label = optional_string(object, "summary", "State " +
                                         std::to_string(id) + " · " + node.kind);
            node.title = node.label;
        } else {
            throw GraphError(context + ": unsupported node kind '" + node.kind + "'");
        }
        model.nodes.push_back(std::move(node));
    }

    const auto& edges = require_array(require_member(root, "edges", "root"),
                                      "edges");
    model.edges.reserve(edges.size());
    std::vector<std::size_t> indegree(model.nodes.size(), 0);
    std::vector<std::vector<std::size_t>> outgoing(model.nodes.size());
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const std::string context = "edges[" + std::to_string(i) + "]";
        const auto& object = require_object(edges[i], context);
        const auto from_id = number_to_size(require_member(object, "from", context),
                                            context + ".from");
        const auto to_id = number_to_size(require_member(object, "to", context),
                                          context + ".to");
        const auto from = index_by_id.find(from_id);
        const auto to = index_by_id.find(to_id);
        if (from == index_by_id.end() || to == index_by_id.end()) {
            throw GraphError(context + ": edge references an unknown node");
        }
        model.edges.push_back({from->second, to->second,
                               optional_string(object, "condition", "true")});
        outgoing[from->second].push_back(to->second);
        ++indegree[to->second];
    }

    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < indegree.size(); ++i) {
        if (indegree[i] == 0) ready.push(i);
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const auto node = ready.front();
        ready.pop();
        ++visited;
        for (const auto child : outgoing[node]) {
            model.nodes[child].depth = std::max(model.nodes[child].depth,
                                                model.nodes[node].depth + 1);
            if (--indegree[child] == 0) ready.push(child);
        }
    }
    if (visited != model.nodes.size()) {
        throw GraphError("input graph contains a directed cycle");
    }
    return model;
}

std::pair<double, double> dag_node_dimensions(const DagVisualNode& node) {
    if (node.kind == "start") return {300, 78};
    if (node.kind == "se") return {360, 132};
    return {360, 122};
}

void layout_dag(DagVisualModel& model) {
    std::size_t max_depth = 0;
    for (const auto& node : model.nodes) max_depth = std::max(max_depth, node.depth);
    std::vector<std::vector<std::size_t>> layers(max_depth + 1);
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        layers[model.nodes[i].depth].push_back(i);
    }
    constexpr double gap_x = 470;
    constexpr double gap_y = 245;
    constexpr double margin_x = 230;
    constexpr double first_y = 155;
    std::size_t widest = 1;
    for (const auto& layer : layers) widest = std::max(widest, layer.size());
    const double width = margin_x * 2 + static_cast<double>(widest - 1) * gap_x;
    for (std::size_t depth = 0; depth < layers.size(); ++depth) {
        const auto& layer = layers[depth];
        const double layer_width = static_cast<double>(layer.size() - 1) * gap_x;
        const double start_x = (width - layer_width) / 2;
        for (std::size_t i = 0; i < layer.size(); ++i) {
            model.nodes[layer[i]].x = start_x + static_cast<double>(i) * gap_x;
            model.nodes[layer[i]].y = first_y + static_cast<double>(depth) * gap_y;
        }
    }
}

std::string render_dag_svg(DagVisualModel model, const DagGraphOptions& options) {
    layout_dag(model);
    std::size_t max_depth = 0;
    std::size_t widest = 1;
    std::map<std::size_t, std::size_t> layer_counts;
    for (const auto& node : model.nodes) {
        max_depth = std::max(max_depth, node.depth);
        widest = std::max(widest, ++layer_counts[node.depth]);
    }
    const double width = std::max(820.0, 460.0 + static_cast<double>(widest - 1) * 470.0);
    const double height = 155.0 + static_cast<double>(max_depth) * 245.0 + 135.0;
    std::ostringstream out;
    out << std::fixed << std::setprecision(1)
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
        << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << ' '
        << height << "\" role=\"img\" aria-labelledby=\"dag-title dag-desc\">\n"
        << "<title id=\"dag-title\">Control-flow DAG for "
        << xml_escape(model.protocol) << "</title>\n"
        << "<desc id=\"dag-desc\">SE states connected by control-flow conditions.</desc>\n"
        << "<defs><marker id=\"dag-arrow\" markerWidth=\"8\" markerHeight=\"8\" "
           "refX=\"7\" refY=\"4\" orient=\"auto\"><path d=\"M0,0 L8,4 L0,8 z\" "
           "fill=\"#7b879b\"/></marker><filter id=\"dag-shadow\" x=\"-15%\" "
           "y=\"-15%\" width=\"130%\" height=\"140%\"><feDropShadow dx=\"0\" "
           "dy=\"3\" stdDeviation=\"3\" flood-opacity=\"0.12\"/></filter></defs>\n"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#f6f8fc\"/>\n"
        << "<text x=\"36\" y=\"38\" font-family=\"ui-sans-serif, system-ui\" "
           "font-size=\"23\" font-weight=\"700\" fill=\"#172033\">"
        << xml_escape(model.protocol) << " · control-flow DAG</text>\n"
        << "<text x=\"36\" y=\"64\" font-family=\"ui-sans-serif, system-ui\" "
           "font-size=\"13\" fill=\"#667085\">" << model.nodes.size()
        << " states · " << model.edges.size() << " edges · " << model.included_paths
        << " paths</text>\n";

    for (const auto& edge : model.edges) {
        const auto& from = model.nodes[edge.from];
        const auto& to = model.nodes[edge.to];
        const auto [from_width, from_height] = dag_node_dimensions(from);
        const auto [to_width, to_height] = dag_node_dimensions(to);
        (void)from_width;
        (void)to_width;
        const double from_y = from.y + from_height / 2;
        const double to_y = to.y - to_height / 2 - 7;
        const double mid_y = (from_y + to_y) / 2;
        out << "<path d=\"M " << from.x << ' ' << from_y << " C " << from.x << ' '
            << mid_y << ", " << to.x << ' ' << mid_y << ", " << to.x << ' '
            << to_y << "\" fill=\"none\" stroke=\"#7b879b\" stroke-width=\"2\" "
               "marker-end=\"url(#dag-arrow)\"/>\n";
        if (!options.show_true_conditions && edge.condition == "true") continue;
        const auto lines = wrap_text(edge.condition, 42, 4);
        std::size_t longest = 4;
        for (const auto& line : lines) longest = std::max(longest, line.size());
        const double label_width = std::min(340.0, 18.0 + static_cast<double>(longest) * 7.2);
        const double label_height = 12.0 + static_cast<double>(lines.size()) * 16.0;
        const double label_x = (from.x + to.x) / 2;
        const double label_y = mid_y;
        out << "<g><title>" << xml_escape(edge.condition) << "</title><rect x=\""
            << label_x - label_width / 2 << "\" y=\"" << label_y - label_height / 2
            << "\" width=\"" << label_width << "\" height=\"" << label_height
            << "\" rx=\"7\" fill=\"#fff8e6\" stroke=\"#d6a145\"/>\n"
            << "<text x=\"" << label_x << "\" y=\""
            << label_y - static_cast<double>(lines.size() - 1) * 8.0 + 4
            << "\" text-anchor=\"middle\" font-family=\"ui-monospace, Menlo, monospace\" "
               "font-size=\"11\" fill=\"#66460e\">";
        for (std::size_t i = 0; i < lines.size(); ++i) {
            out << "<tspan x=\"" << label_x << "\" dy=\"" << (i == 0 ? 0 : 16)
                << "\">" << xml_escape(lines[i]) << "</tspan>";
        }
        out << "</text></g>\n";
    }

    for (const auto& node : model.nodes) {
        const auto [node_width, node_height] = dag_node_dimensions(node);
        const double left = node.x - node_width / 2;
        const double top = node.y - node_height / 2;
        std::string fill = "#e7f0ff";
        std::string stroke = "#3973cf";
        if (node.kind == "start") { fill = "#172033"; stroke = "#172033"; }
        else if (node.kind == "terminal") { fill = "#e5f7ed"; stroke = "#258a55"; }
        else if (node.kind == "error") { fill = "#fde8e7"; stroke = "#c43d36"; }
        else if (node.kind == "bound") { fill = "#fff0df"; stroke = "#d16718"; }
        out << "<g id=\"state-" << node.source_id << "\" data-kind=\""
            << xml_escape(node.kind) << "\"><title>" << xml_escape(node.title)
            << "</title><rect x=\"" << left << "\" y=\"" << top << "\" width=\""
            << node_width << "\" height=\"" << node_height << "\" rx=\"12\" fill=\""
            << fill << "\" stroke=\"" << stroke
            << "\" stroke-width=\"2\" filter=\"url(#dag-shadow)\"/>\n";
        const auto lines = wrap_text(node.label, 45, node.kind == "se" ? 5 : 4);
        const double first_line = node.y - static_cast<double>(lines.size() - 1) * 9.0 + 5;
        out << "<text x=\"" << node.x << "\" y=\"" << first_line
            << "\" text-anchor=\"middle\" font-family=\"ui-monospace, Menlo, monospace\" "
               "font-size=\"12\" fill=\""
            << (node.kind == "start" ? "#ffffff" : "#172033") << "\">";
        for (std::size_t i = 0; i < lines.size(); ++i) {
            out << "<tspan x=\"" << node.x << "\" dy=\"" << (i == 0 ? 0 : 18) << '"'
                << (i == 0 ? " font-weight=\"700\"" : "") << '>'
                << xml_escape(lines[i]) << "</tspan>";
        }
        out << "</text></g>\n";
    }
    out << "</svg>\n";
    return out.str();
}

std::string render_dag_dot(const DagVisualModel& model,
                           const DagGraphOptions& options) {
    std::ostringstream out;
    out << "digraph protocol_dag {\n"
        << "  graph [rankdir=TB, bgcolor=\"#f6f8fc\", pad=0.4, nodesep=0.45, "
           "ranksep=0.9, labelloc=t, label=\"" << dot_escape(model.protocol)
        << " · control-flow DAG\"];\n"
        << "  node [shape=box, style=\"rounded,filled\", fontname=\"Menlo\", "
           "fontsize=10, color=\"#3973cf\", fillcolor=\"#e7f0ff\"];\n"
        << "  edge [color=\"#7b879b\", fontname=\"Menlo\", fontsize=9];\n";
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        std::string attributes;
        if (node.kind == "start") {
            attributes = ", shape=oval, color=\"#172033\", fillcolor=\"#172033\", fontcolor=white";
        } else if (node.kind == "terminal") {
            attributes = ", color=\"#258a55\", fillcolor=\"#e5f7ed\"";
        } else if (node.kind == "error") {
            attributes = ", color=\"#c43d36\", fillcolor=\"#fde8e7\"";
        } else if (node.kind == "bound") {
            attributes = ", color=\"#d16718\", fillcolor=\"#fff0df\"";
        }
        out << "  n" << i << " [label=\"" << dot_escape(node.label) << '"'
            << attributes << "];\n";
    }
    for (const auto& edge : model.edges) {
        out << "  n" << edge.from << " -> n" << edge.to;
        if (options.show_true_conditions || edge.condition != "true") {
            out << " [label=\"" << dot_escape(edge.condition) << "\"]";
        }
        out << ";\n";
    }
    out << "}\n";
    return out.str();
}

} // namespace

GraphResult render_path_graph_json(std::string_view json, GraphFormat format,
                                   const GraphOptions& options) {
    JsonParser parser(json);
    GraphModel graph = build_graph(parser.parse(), options);
    GraphResult result;
    result.protocol_name = graph.protocol;
    result.input_path_count = graph.input_paths;
    result.rendered_path_count = graph.rendered_paths;
    result.input_truncated = graph.input_truncated;
    result.render_truncated = graph.render_truncated;
    if (format == GraphFormat::Svg) result.content = render_svg(std::move(graph));
    else if (format == GraphFormat::Dot) result.content = render_dot(graph);
    else result.content = render_dag_json(graph);
    return result;
}

GraphResult render_path_graph_file(const std::filesystem::path& path,
                                   GraphFormat format,
                                   const GraphOptions& options) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw GraphError("cannot open input file " + path.string());
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        throw GraphError("failed while reading input file " + path.string());
    }
    return render_path_graph_json(buffer.str(), format, options);
}

GraphResult render_dag_graph_json(std::string_view json, GraphFormat format,
                                  const DagGraphOptions& options) {
    if (format == GraphFormat::DagJson) {
        throw GraphError("DAG visualization output format must be SVG or DOT");
    }
    JsonParser parser(json);
    DagVisualModel model = parse_dag_model(parser.parse());
    GraphResult result;
    result.protocol_name = model.protocol;
    result.input_path_count = model.input_paths;
    result.rendered_path_count = model.included_paths;
    result.input_truncated = model.input_truncated;
    result.render_truncated = model.dag_truncated;
    result.content = format == GraphFormat::Svg
        ? render_dag_svg(std::move(model), options)
        : render_dag_dot(model, options);
    return result;
}

GraphResult render_dag_graph_file(const std::filesystem::path& path,
                                  GraphFormat format,
                                  const DagGraphOptions& options) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw GraphError("cannot open input file " + path.string());
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        throw GraphError("failed while reading input file " + path.string());
    }
    return render_dag_graph_json(buffer.str(), format, options);
}

} // namespace fpdl
