#include "fpdl/parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace fpdl {
namespace {

std::string json_escape(std::string_view text) {
    std::ostringstream out;
    for (const unsigned char ch : text) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

enum class TokenKind { End, Identifier, Number, String, Null, Symbol };

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    SourceLocation location;
};

using NodeId = std::size_t;

struct SyntaxEdge {
    std::string label;
    NodeId target = 0;
};

struct SyntaxNode {
    NodeId id = 0;
    std::string kind;
    std::string value;
    SourceLocation location;
    std::vector<SyntaxEdge> edges;
};

class SyntaxTree {
public:
    NodeId add(std::string kind, std::string value,
               std::vector<SyntaxEdge> edges, SourceLocation location) {
        const NodeId id = nodes_.size();
        nodes_.push_back({id, std::move(kind), std::move(value), location,
                          std::move(edges)});
        return id;
    }

    const SyntaxNode& node(NodeId id) const {
        if (id >= nodes_.size()) throw std::out_of_range("syntax node ID out of range");
        return nodes_[id];
    }

private:
    std::vector<SyntaxNode> nodes_;
};

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}

    std::vector<Token> run() {
        std::vector<Token> tokens;
        while (!at_end()) {
            skip_space_and_comments();
            if (at_end()) break;

            const SourceLocation start{line_, column_};
            if (starts_with("#-flag")) {
                advance_n(6);
                tokens.push_back({TokenKind::Identifier, "#-flag", start});
            } else if (starts_with("⊥")) {
                advance_n(std::string_view("⊥").size());
                tokens.push_back({TokenKind::Null, "⊥", start});
            } else if (is_identifier_start(peek())) {
                tokens.push_back(identifier(start));
            } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
                tokens.push_back(number(start));
            } else if (peek() == '"') {
                tokens.push_back(string(start));
            } else {
                tokens.push_back(symbol(start));
            }
        }
        tokens.push_back({TokenKind::End, "<eof>", {line_, column_}});
        return tokens;
    }

private:
    static bool is_identifier_start(char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
    }

    static bool is_identifier_continue(char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
    }

    bool at_end() const { return offset_ >= source_.size(); }
    char peek(std::size_t lookahead = 0) const {
        const auto pos = offset_ + lookahead;
        return pos < source_.size() ? source_[pos] : '\0';
    }
    bool starts_with(std::string_view text) const {
        return source_.substr(offset_, text.size()) == text;
    }

    char advance() {
        const char ch = source_[offset_++];
        if (ch == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return ch;
    }

    void advance_n(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) advance();
    }

    void skip_space_and_comments() {
        for (;;) {
            while (!at_end() && std::isspace(static_cast<unsigned char>(peek()))) {
                advance();
            }
            if (starts_with("//")) {
                while (!at_end() && peek() != '\n') advance();
                continue;
            }
            break;
        }
    }

    Token identifier(SourceLocation start) {
        std::string text;
        while (!at_end() && is_identifier_continue(peek())) text += advance();
        return {TokenKind::Identifier, std::move(text), start};
    }

    Token number(SourceLocation start) {
        std::string text;
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            text += advance();
        }
        return {TokenKind::Number, std::move(text), start};
    }

    Token string(SourceLocation start) {
        advance(); // opening quote
        std::string text;
        while (!at_end() && peek() != '"') {
            if (peek() == '\n') throw ParseError(start, "newline in string literal");
            if (peek() == '\\') {
                advance();
                if (at_end()) throw ParseError(start, "unterminated string literal");
                const char escaped = advance();
                switch (escaped) {
                case 'n': text += '\n'; break;
                case 'r': text += '\r'; break;
                case 't': text += '\t'; break;
                case '\\': text += '\\'; break;
                case '"': text += '"'; break;
                default: text += escaped; break;
                }
            } else {
                text += advance();
            }
        }
        if (at_end()) throw ParseError(start, "unterminated string literal");
        advance();
        return {TokenKind::String, std::move(text), start};
    }

    Token symbol(SourceLocation start) {
        static constexpr std::string_view two_char[] = {
            "==", "!=", "<=", ">=", "++", "+="
        };
        for (const auto candidate : two_char) {
            if (starts_with(candidate)) {
                advance_n(candidate.size());
                return {TokenKind::Symbol, std::string(candidate), start};
            }
        }

        static constexpr std::string_view valid = ":;{}[](),.+-<>!=";
        const char ch = advance();
        if (valid.find(ch) == std::string_view::npos) {
            throw ParseError(start, std::string("unexpected character '") + ch + "'");
        }
        return {TokenKind::Symbol, std::string(1, ch), start};
    }

    std::string_view source_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

struct ParsedType {
    NodeId node = 0;
    std::string spelling;
};

class ParserImpl {
public:
    ParserImpl(std::string_view source, std::filesystem::path source_path,
               ParseOptions options)
        : tokens_(Lexer(source).run()), source_path_(std::move(source_path)),
          options_(options) {}

    ParseResult run() {
        const auto start = current().location;
        if (!options_.bmc_bound || *options_.bmc_bound == 0) {
            fail_at(start, "symbolic path expansion requires a positive BMC bound");
        }
        if (options_.max_paths == 0) fail_at(start, "max_paths must be positive");
        expect("protocol");
        std::string protocol_name;
        while (!check(":")) {
            if (current().kind == TokenKind::End) fail("expected ':' after protocol name");
            protocol_name += advance().text;
        }
        expect(":");

        std::vector<SyntaxEdge> sections;
        sections.push_back({"code", parse_code()});
        sections.push_back({"se", parse_se()});
        sections.push_back({"gvar", parse_gvar()});
        sections.push_back({"tc", parse_tc()});
        sections.push_back({"af", parse_af()});
        sections.push_back({"tp", parse_tp()});
        expect_kind(TokenKind::End, "end of file");

        if (tc_ids_ != tp_ids_) {
            fail_at(start, "terminal-condition IDs and terminating-policy IDs differ");
        }
        const NodeId root = make("Protocol", protocol_name, std::move(sections), start);
        auto paths = expand_paths(root);
        for (std::size_t i = 0; i < paths.size(); ++i) paths[i].id = i;
        diagnostics_.push_back({Diagnostic::Severity::Warning, start,
            "symbolic constraints are not SMT-solved; only constant and exact-opposite branches are pruned"});
        return {protocol_name, std::move(paths), std::move(diagnostics_), truncated_};
    }

private:
    const Token& current(std::size_t lookahead = 0) const {
        const auto index = std::min(position_ + lookahead, tokens_.size() - 1);
        return tokens_[index];
    }

    bool check(std::string_view text) const { return current().text == text; }
    bool check_kind(TokenKind kind) const { return current().kind == kind; }
    bool section(std::string_view name) const {
        return current().kind == TokenKind::Identifier && current().text == name &&
               current(1).text == ":";
    }

    Token advance() {
        const Token token = current();
        if (position_ < tokens_.size() - 1) ++position_;
        return token;
    }

    bool match(std::string_view text) {
        if (!check(text)) return false;
        advance();
        return true;
    }

    Token expect(std::string_view text) {
        if (!check(text)) fail("expected '" + std::string(text) + "', found '" + current().text + "'");
        return advance();
    }

    Token expect_kind(TokenKind kind, std::string_view description) {
        if (!check_kind(kind)) {
            fail("expected " + std::string(description) + ", found '" + current().text + "'");
        }
        return advance();
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw ParseError(current().location, message);
    }

    [[noreturn]] void fail_at(SourceLocation location, const std::string& message) const {
        throw ParseError(location, message);
    }

    NodeId make(std::string kind, std::string value, std::vector<SyntaxEdge> edges,
                SourceLocation location) {
        return ast_.add(std::move(kind), std::move(value), std::move(edges), location);
    }

    NodeId leaf(std::string kind, std::string value, SourceLocation location) {
        return make(std::move(kind), std::move(value), {}, location);
    }

    NodeId sequence(std::string kind, const std::vector<NodeId>& children,
                    SourceLocation location) {
        std::vector<SyntaxEdge> edges;
        edges.reserve(children.size());
        for (std::size_t i = 0; i < children.size(); ++i) {
            edges.push_back({std::to_string(i), children[i]});
        }
        return make(std::move(kind), "", std::move(edges), location);
    }

    NodeId parse_code() {
        const auto start = expect("code").location;
        expect(":");
        const auto parameters = parse_primary();
        const auto generators = parse_primary();
        return make("Code", "", {{"parameters", parameters}, {"generators", generators}}, start);
    }

    NodeId parse_se() {
        const auto start = expect("se").location;
        expect(":");
        std::vector<NodeId> definitions;
        while (!section("gvar")) definitions.push_back(parse_se_definition());
        return sequence("SESection", definitions, start);
    }

    struct QasmRegisters {
        std::unordered_map<std::string, std::size_t> classical;
    };

    QasmRegisters inspect_qasm(const std::filesystem::path& path, SourceLocation location) {
        std::ifstream input(path);
        if (!input) fail_at(location, "cannot open referenced QASM file '" + path.string() + "'");
        const std::string source((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
        const std::regex declaration(R"(\bbit\s*(?:\[\s*([0-9]+)\s*\])?\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");
        QasmRegisters result;
        for (auto it = std::sregex_iterator(source.begin(), source.end(), declaration);
             it != std::sregex_iterator(); ++it) {
            const std::size_t width = (*it)[1].matched
                ? static_cast<std::size_t>(std::stoull((*it)[1].str())) : 1;
            result.classical[(*it)[2].str()] = width;
        }
        return result;
    }

    NodeId parse_se_definition() {
        const auto name = expect_kind(TokenKind::Identifier, "SE name");
        if (!se_names_.insert(name.text).second) fail_at(name.location, "duplicate SE '" + name.text + "'");
        expect("{");

        std::vector<SyntaxEdge> fields;
        std::set<std::string> seen_fields;
        std::string file;
        std::string cm;
        std::string cf;
        bool has_qf = false;
        bool has_flag_claim = false;
        while (!match("}")) {
            const auto key = expect_kind(TokenKind::Identifier, "SE field name");
            static const std::set<std::string> valid_fields{
                "file", "qp", "qm", "cm", "qf", "cf", "g", "#-flag"
            };
            if (!valid_fields.contains(key.text)) fail_at(key.location, "unknown SE field '" + key.text + "'");
            if (!seen_fields.insert(key.text).second) fail_at(key.location, "duplicate SE field '" + key.text + "'");
            expect(":");
            NodeId value = 0;
            if (key.text == "file") {
                const auto token = expect_kind(TokenKind::String, "QASM path string");
                file = token.text;
                value = leaf("String", token.text, token.location);
            } else if (key.text == "g") {
                value = parse_primary();
            } else if (key.text == "#-flag") {
                const auto token = expect_kind(TokenKind::Number, "flag bound");
                has_flag_claim = true;
                value = leaf("Integer", token.text, token.location);
            } else {
                const auto token = expect_kind(TokenKind::Identifier, "register name");
                if (key.text == "cm") cm = token.text;
                if (key.text == "cf") cf = token.text;
                if (key.text == "qf") has_qf = true;
                value = leaf("Identifier", token.text, token.location);
            }
            fields.push_back({key.text, value});
        }

        for (const std::string required : {"file", "qp", "qm", "cm", "g"}) {
            if (!seen_fields.contains(required)) fail_at(name.location, "SE '" + name.text + "' has no " + required + " field");
        }
        const bool flagged = has_flag_claim || has_qf || !cf.empty();
        if (flagged && !(has_flag_claim && has_qf && !cf.empty())) {
            fail_at(name.location, "flagged SE '" + name.text + "' must define qf, cf, and #-flag together");
        }
        const std::size_t arity = flagged ? 2 : 1;
        std::vector<SyntaxEdge> signature_edges;
        if (options_.inspect_qasm_registers && !file.empty()) {
            const auto resolved = source_path_.parent_path() / file;
            const auto registers = inspect_qasm(resolved.lexically_normal(), name.location);
            const auto add_result = [&](std::string_view role, const std::string& register_name) {
                const auto found = registers.classical.find(register_name);
                if (found == registers.classical.end()) {
                    fail_at(name.location, "QASM classical register '" + register_name + "' not found for SE '" + name.text + "'");
                }
                const std::string type = found->second == 1 ? "bit" : "bit[" + std::to_string(found->second) + "]";
                signature_edges.push_back({std::string(role), leaf("Type", type, name.location)});
            };
            if (cm.empty()) fail_at(name.location, "SE '" + name.text + "' has no cm field");
            add_result("syndrome", cm);
            if (arity == 2) {
                if (cf.empty()) fail_at(name.location, "flagged SE '" + name.text + "' has no cf field");
                add_result("flag", cf);
            }
        }
        se_arity_[name.text] = arity;
        if (!signature_edges.empty()) {
            fields.push_back({"return_signature", make("TupleType", "", std::move(signature_edges), name.location)});
        }
        return make("SEDefinition", name.text, std::move(fields), name.location);
    }

    NodeId parse_gvar() {
        const auto start = expect("gvar").location;
        expect(":");
        std::vector<NodeId> declarations;
        parsing_globals_ = true;
        while (!section("tc")) declarations.push_back(parse_declaration());
        parsing_globals_ = false;
        return sequence("GlobalVariables", declarations, start);
    }

    NodeId parse_tc() {
        const auto start = expect("tc").location;
        expect(":");
        std::vector<NodeId> entries;
        parsing_tc_ = true;
        while (!section("af")) {
            const auto id = expect_kind(TokenKind::Number, "terminal-condition ID");
            expect(":");
            const auto expression = parse_expression();
            if (!tc_ids_.insert(id.text).second) fail_at(id.location, "duplicate terminal-condition ID " + id.text);
            entries.push_back(make("TerminalCondition", id.text, {{"condition", expression}}, id.location));
        }
        parsing_tc_ = false;
        return sequence("TerminalConditions", entries, start);
    }

    NodeId parse_af() {
        const auto start = expect("af").location;
        expect(":");
        expect("while");
        expect("(");
        expect("!");
        expect("tc");
        expect(")");
        ++loop_depth_;
        const auto body = parse_block(false);
        --loop_depth_;
        return make("AdaptiveFlow", "", {{"body", body}}, start);
    }

    NodeId parse_tp() {
        const auto start = expect("tp").location;
        expect(":");
        std::vector<NodeId> entries;
        in_tp_ = true;
        while (!check_kind(TokenKind::End)) {
            const auto id = expect_kind(TokenKind::Number, "terminating-policy ID");
            expect(":");
            const auto body = parse_block(false);
            if (!tp_ids_.insert(id.text).second) fail_at(id.location, "duplicate terminating-policy ID " + id.text);
            const auto implicit_end = make("ImplicitEnd", "", {}, id.location);
            entries.push_back(make("TerminatingPolicy", id.text,
                                   {{"body", body}, {"fallthrough", implicit_end}}, id.location));
        }
        in_tp_ = false;
        return sequence("TerminatingPolicies", entries, start);
    }

    bool type_start() const {
        if (current().kind != TokenKind::Identifier) return false;
        return current().text == "bit" || current().text == "cnt" ||
               current().text == "mr" || current().text == "se";
    }

    ParsedType parse_type() {
        const auto start = current().location;
        const auto base = expect_kind(TokenKind::Identifier, "type");
        if (base.text != "bit" && base.text != "cnt" && base.text != "mr" && base.text != "se") {
            fail_at(base.location, "unknown type '" + base.text + "'");
        }
        std::string spelling = base.text;
        std::vector<SyntaxEdge> edges;
        if (base.text == "se" && match("<")) {
            spelling += "<(";
            expect("(");
            std::size_t index = 0;
            do {
                auto component = parse_type();
                if (index++) spelling += ",";
                spelling += component.spelling;
                edges.push_back({"return" + std::to_string(index - 1), component.node});
            } while (match(","));
            expect(")");
            expect(">");
            spelling += ")>";
        }
        while (match("[")) {
            spelling += "[";
            if (check_kind(TokenKind::Number)) spelling += advance().text;
            expect("]");
            spelling += "]";
        }
        return {make("Type", spelling, std::move(edges), start), spelling};
    }

    NodeId parse_declaration() {
        const auto start = current().location;
        const auto type = parse_type();
        const auto name = expect_kind(TokenKind::Identifier, "variable name");
        if (parsing_globals_ && !global_names_.insert(name.text).second) {
            fail_at(name.location, "duplicate global variable '" + name.text + "'");
        }
        std::vector<SyntaxEdge> edges{{"type", type.node}};
        if (match("=")) edges.push_back({"initializer", parse_expression()});
        expect(";");
        return make("Declaration", name.text, std::move(edges), start);
    }

    NodeId parse_block(bool terminating_policy) {
        const auto start = expect("{").location;
        std::vector<NodeId> statements;
        bool definitely_terminated = false;
        while (!match("}")) {
            if (definitely_terminated) fail("reachable syntax follows non-returning terminal action");
            auto statement = parse_statement(terminating_policy);
            const auto& node = ast_.node(statement);
            if (node.kind == "Decode" || node.kind == "End") definitely_terminated = true;
            statements.push_back(statement);
        }
        (void)terminating_policy;
        return sequence("Block", statements, start);
    }

    NodeId parse_statement(bool terminating_policy) {
        if (type_start() && !(check("mr") && current(1).text == "[")) return parse_declaration();
        if (check("if")) return parse_if(terminating_policy);
        if (check("switch")) return parse_switch(terminating_policy);
        if (check("while")) return parse_while(terminating_policy);

        if (match("break")) {
            const auto location = tokens_[position_ - 1].location;
            if (loop_depth_ == 0 && switch_depth_ == 0) {
                fail_at(location, "break is valid only inside while or switch");
            }
            expect(";");
            return make("Break", "", {}, location);
        }
        if (match("continue")) {
            const auto location = tokens_[position_ - 1].location;
            if (loop_depth_ == 0) fail_at(location, "continue is valid only inside while");
            expect(";");
            return make("Continue", "", {}, location);
        }
        if (check("check")) {
            const auto location = advance().location;
            if (in_tp_) fail_at(location, "check() is forbidden in a terminating policy");
            expect("("); expect(")"); expect(";");
            return make("CheckTerminalConditions", "", {}, location);
        }
        if (check("decode")) {
            const auto location = advance().location;
            if (!in_tp_) fail_at(location, "decode() is valid only in a terminating policy");
            expect("(");
            const auto record = parse_expression();
            expect(")"); expect(";");
            return make("Decode", "", {{"record", record}}, location);
        }
        if (check("end")) {
            const auto location = advance().location;
            if (!in_tp_) fail_at(location, "end() is valid only in a terminating policy");
            expect("("); expect(")"); expect(";");
            return make("End", "", {}, location);
        }
        if (check("(")) return parse_tuple_assignment();
        if (check_kind(TokenKind::Identifier)) return parse_simple_statement();
        fail("expected statement");
    }

    NodeId parse_if(bool terminating_policy) {
        (void)terminating_policy;
        const auto start = expect("if").location;
        expect("(");
        const auto condition = parse_expression();
        expect(")");
        const auto then_block = parse_block(false);
        std::vector<SyntaxEdge> edges{{"condition", condition}, {"then", then_block}};
        if (match("else")) {
            if (check("if")) edges.push_back({"else", parse_if(false)});
            else edges.push_back({"else", parse_block(false)});
        }
        return make("If", "", std::move(edges), start);
    }

    NodeId parse_switch(bool terminating_policy) {
        (void)terminating_policy;
        const auto start = expect("switch").location;
        expect("(");
        const auto selector = parse_expression();
        expect(")"); expect("{");
        ++switch_depth_;
        std::vector<SyntaxEdge> edges{{"selector", selector}};
        std::size_t case_index = 0;
        while (!match("}")) {
            if (match("default")) {
                expect(":");
                edges.push_back({"default", parse_block(false)});
            } else {
                const auto value = parse_expression();
                expect(":");
                const auto body = parse_block(false);
                edges.push_back({"case" + std::to_string(case_index++),
                    make("Case", "", {{"value", value}, {"body", body}}, start)});
            }
        }
        --switch_depth_;
        return make("Switch", "no-fallthrough", std::move(edges), start);
    }

    NodeId parse_while(bool terminating_policy) {
        (void)terminating_policy;
        const auto start = expect("while").location;
        expect("(");
        const auto condition = parse_expression();
        expect(")");
        ++loop_depth_;
        const auto body = parse_block(false);
        --loop_depth_;
        return make("While", "", {{"condition", condition}, {"body", body}}, start);
    }

    struct ParsedCall {
        NodeId node = 0;
        std::string direct_name;
    };

    ParsedCall parse_call() {
        const auto start = current().location;
        auto callee = parse_primary();
        std::string direct_name;
        if (ast_.node(callee).kind == "Identifier") direct_name = ast_.node(callee).value;
        while (match("[")) {
            const auto index = parse_expression();
            expect("]");
            callee = make("Index", "", {{"base", callee}, {"index", index}}, start);
            direct_name.clear();
        }
        expect("("); expect(")");
        return {make("SECall", "", {{"callee", callee}}, start), direct_name};
    }

    NodeId parse_tuple_assignment() {
        const auto start = expect("(").location;
        std::vector<NodeId> targets;
        targets.push_back(leaf("Identifier", expect_kind(TokenKind::Identifier, "tuple target").text, start));
        bool saw_comma = false;
        while (match(",")) {
            saw_comma = true;
            if (check(")")) break;
            const auto target = expect_kind(TokenKind::Identifier, "tuple target");
            targets.push_back(leaf("Identifier", target.text, target.location));
        }
        if (!saw_comma) fail_at(start, "single-element SE tuple must use a trailing comma, for example (s,)");
        expect(")"); expect("=");
        const auto call = parse_call();
        expect(";");
        if (!call.direct_name.empty()) {
            const auto found = se_arity_.find(call.direct_name);
            if (found == se_arity_.end()) fail_at(start, "call to unknown SE '" + call.direct_name + "'");
            if (found->second != targets.size()) {
                fail_at(start, "SE '" + call.direct_name + "' returns " + std::to_string(found->second) +
                               " values, but assignment has " + std::to_string(targets.size()) + " targets");
            }
        }
        return make("SETupleAssignment", "",
                    {{"targets", sequence("TuplePattern", targets, start)}, {"call", call.node}}, start);
    }

    NodeId parse_simple_statement() {
        const auto name = expect_kind(TokenKind::Identifier, "assignment target");
        NodeId lhs = leaf("Identifier", name.text, name.location);
        while (match("[")) {
            const auto index = parse_expression();
            expect("]");
            lhs = make("Index", "", {{"base", lhs}, {"index", index}}, name.location);
        }
        if (match(".")) {
            expect("push"); expect("(");
            const auto value = parse_expression();
            expect(")"); expect(";");
            return make("Push", "", {{"target", lhs}, {"value", value}}, name.location);
        }
        if (match("++")) {
            expect(";");
            return make("Increment", "", {{"target", lhs}}, name.location);
        }
        if (match("+=")) {
            const auto rhs = parse_expression(); expect(";");
            return make("AddAssign", "", {{"lhs", lhs}, {"rhs", rhs}}, name.location);
        }
        expect("=");
        if (ast_.node(lhs).kind == "Index") {
            fail_at(name.location, "indexed assignment is not part of FPDL; use an arbitrary-length vector and push()");
        }
        const auto rhs = parse_expression(); expect(";");
        return make("Assign", "", {{"lhs", lhs}, {"rhs", rhs}}, name.location);
    }

    NodeId parse_expression() { return parse_or(); }

    NodeId parse_or() {
        auto lhs = parse_and();
        while (match("or")) {
            const auto location = tokens_[position_ - 1].location;
            const auto rhs = parse_and();
            lhs = make("Binary", "or", {{"lhs", lhs}, {"rhs", rhs}}, location);
        }
        return lhs;
    }

    NodeId parse_and() {
        auto lhs = parse_comparison();
        while (match("and")) {
            const auto location = tokens_[position_ - 1].location;
            const auto rhs = parse_comparison();
            lhs = make("Binary", "and", {{"lhs", lhs}, {"rhs", rhs}}, location);
        }
        return lhs;
    }

    NodeId parse_comparison() {
        auto lhs = parse_additive();
        while (check("==") || check("!=") || check("<") || check(">") ||
               check("<=") || check(">=")) {
            const auto op = advance();
            const auto rhs = parse_additive();
            lhs = make("Binary", op.text, {{"lhs", lhs}, {"rhs", rhs}}, op.location);
        }
        return lhs;
    }

    NodeId parse_additive() {
        auto lhs = parse_postfix();
        while (check("+") || check("-")) {
            const auto op = advance();
            const auto rhs = parse_postfix();
            lhs = make("Binary", op.text, {{"lhs", lhs}, {"rhs", rhs}}, op.location);
        }
        return lhs;
    }

    NodeId parse_postfix() {
        const auto start = current().location;
        auto value = parse_primary();
        while (match("[")) {
            const auto index = parse_expression();
            expect("]");
            value = make("Index", "", {{"base", value}, {"index", index}}, start);
        }
        return value;
    }

    NodeId parse_primary() {
        const auto token = current();
        if (match("(")) {
            const auto expression = parse_expression();
            expect(")");
            return expression;
        }
        if (match("[")) return parse_list("VectorLiteral", token.location, "]");
        if (match("{")) return parse_list("BitConcatenation", token.location, "}");
        if (check("mr") && current(1).text == "[") {
            advance(); expect("[");
            return parse_list("MeasurementRecordLiteral", token.location, "]");
        }
        if (check_kind(TokenKind::Number)) {
            advance();
            return leaf("Integer", token.text, token.location);
        }
        if (check_kind(TokenKind::Null)) {
            advance();
            return leaf("Null", token.text, token.location);
        }
        if (check_kind(TokenKind::String)) {
            advance();
            return leaf("String", token.text, token.location);
        }
        if (check_kind(TokenKind::Identifier)) {
            advance();
            if (token.text == "true" || token.text == "false") {
                return leaf("Boolean", token.text, token.location);
            }
            if (parsing_tc_ && !global_names_.contains(token.text)) {
                fail_at(token.location, "terminal condition may reference only global variables; '" + token.text + "' is not global");
            }
            return leaf("Identifier", token.text, token.location);
        }
        fail("expected expression");
    }

    NodeId parse_list(std::string kind, SourceLocation location, std::string_view closing) {
        std::vector<NodeId> values;
        if (!check(closing)) {
            do {
                values.push_back(parse_expression());
            } while (match(",") && !check(closing));
        }
        expect(closing);
        return sequence(std::move(kind), values, location);
    }

    struct SymbolicValue {
        std::string text;
        std::optional<long long> integer;
        std::optional<bool> boolean;
        bool is_null = false;
    };

    enum class Signal { Normal, Break, Continue, Terminated, Bound, Error };

    struct ExecutionState {
        SymbolicPath path;
        std::unordered_map<std::string, SymbolicValue> values;
        std::unordered_map<std::string, std::vector<SymbolicValue>> vectors;
        std::size_t steps = 0;
        std::size_t round = 0;
        std::size_t invocation = 0;
        std::string phase = "af";
        Signal signal = Signal::Normal;
    };

    const SyntaxNode& syntax(NodeId id) const { return ast_.node(id); }

    std::optional<NodeId> optional_edge(NodeId id, std::string_view label) const {
        for (const auto& edge : syntax(id).edges) {
            if (edge.label == label) return edge.target;
        }
        return std::nullopt;
    }

    NodeId required_edge(NodeId id, std::string_view label) const {
        if (const auto found = optional_edge(id, label)) return *found;
        throw std::logic_error("missing syntax edge '" + std::string(label) + "'");
    }

    std::vector<NodeId> children(NodeId id) const {
        std::vector<NodeId> result;
        result.reserve(syntax(id).edges.size());
        for (const auto& edge : syntax(id).edges) result.push_back(edge.target);
        return result;
    }

    bool consume(ExecutionState& state) const {
        if (state.steps >= *options_.bmc_bound) {
            state.signal = Signal::Bound;
            state.path.bound_exceeded = true;
            state.path.transitions = state.steps;
            return false;
        }
        ++state.steps;
        state.path.transitions = state.steps;
        return true;
    }

    template <typename T>
    void cap(std::vector<T>& values) {
        if (values.size() > options_.max_paths) {
            values.resize(options_.max_paths);
            truncated_ = true;
        }
    }

    static std::string parenthesize(const SymbolicValue& value) {
        return "(" + value.text + ")";
    }

    SymbolicValue evaluate(NodeId id, const ExecutionState& state) const {
        const auto& node = syntax(id);
        if (node.kind == "Integer") {
            return {node.value, std::stoll(node.value), std::nullopt, false};
        }
        if (node.kind == "Boolean") {
            return {node.value, std::nullopt, node.value == "true", false};
        }
        if (node.kind == "Null") return {"⊥", std::nullopt, std::nullopt, true};
        if (node.kind == "String") return {"\"" + node.value + "\"", {}, {}, false};
        if (node.kind == "Identifier") {
            if (const auto found = state.values.find(node.value); found != state.values.end()) {
                const auto& symbolic = found->second;
                if (!symbolic.integer && !symbolic.boolean && !symbolic.is_null) {
                    for (const auto& constraint : state.path.constraints) {
                        if (!constraint.expected) continue;
                        if (constraint.expression == "(" + symbolic.text + ") == (0)") {
                            return {"0", 0, {}, false};
                        }
                        if (constraint.expression == "(" + symbolic.text + ") == (1)") {
                            return {"1", 1, {}, false};
                        }
                        if (constraint.expression == "(" + symbolic.text + ") == (⊥)") {
                            return {"⊥", {}, {}, true};
                        }
                    }
                }
                return symbolic;
            }
            return {node.value, {}, {}, false};
        }
        if (node.kind == "Index") {
            const auto base_id = required_edge(id, "base");
            const auto index = evaluate(required_edge(id, "index"), state);
            const auto& base_node = syntax(base_id);
            if (base_node.kind == "Identifier" && index.integer && *index.integer >= 0) {
                if (const auto found = state.vectors.find(base_node.value);
                    found != state.vectors.end() &&
                    static_cast<std::size_t>(*index.integer) < found->second.size()) {
                    return found->second[static_cast<std::size_t>(*index.integer)];
                }
            }
            const auto base = evaluate(base_id, state);
            return {base.text + "[" + index.text + "]", {}, {}, false};
        }
        if (node.kind == "Binary") {
            const auto lhs = evaluate(required_edge(id, "lhs"), state);
            const auto rhs = evaluate(required_edge(id, "rhs"), state);
            const std::string text = parenthesize(lhs) + " " + node.value + " " + parenthesize(rhs);
            if (node.value == "+" && lhs.integer && rhs.integer) {
                const auto value = *lhs.integer + *rhs.integer;
                return {std::to_string(value), value, {}, false};
            }
            if (node.value == "-" && lhs.integer && rhs.integer) {
                const auto value = *lhs.integer - *rhs.integer;
                return {std::to_string(value), value, {}, false};
            }
            if (node.value == "and") {
                if ((lhs.boolean && !*lhs.boolean) || (rhs.boolean && !*rhs.boolean)) return {"false", {}, false, false};
                if (lhs.boolean && rhs.boolean) return {*lhs.boolean && *rhs.boolean ? "true" : "false", {}, *lhs.boolean && *rhs.boolean, false};
            }
            if (node.value == "or") {
                if ((lhs.boolean && *lhs.boolean) || (rhs.boolean && *rhs.boolean)) return {"true", {}, true, false};
                if (lhs.boolean && rhs.boolean) return {*lhs.boolean || *rhs.boolean ? "true" : "false", {}, *lhs.boolean || *rhs.boolean, false};
            }
            std::optional<bool> comparison;
            if (node.value == "==" || node.value == "!=") {
                if (lhs.is_null || rhs.is_null) comparison = lhs.is_null && rhs.is_null;
                else if (lhs.integer && rhs.integer) comparison = *lhs.integer == *rhs.integer;
                else if (lhs.boolean && rhs.boolean) comparison = *lhs.boolean == *rhs.boolean;
                else if (lhs.text == rhs.text) comparison = true;
                if (!comparison) {
                    const std::string equality = "(" + lhs.text + ") == (" + rhs.text + ")";
                    for (const auto& constraint : state.path.constraints) {
                        if (constraint.expression == equality) {
                            comparison = constraint.expected;
                            break;
                        }
                    }
                }
                if (comparison && node.value == "!=") comparison = !*comparison;
            } else if (lhs.integer && rhs.integer) {
                if (node.value == "<") comparison = *lhs.integer < *rhs.integer;
                else if (node.value == ">") comparison = *lhs.integer > *rhs.integer;
                else if (node.value == "<=") comparison = *lhs.integer <= *rhs.integer;
                else if (node.value == ">=") comparison = *lhs.integer >= *rhs.integer;
            }
            if (comparison) return {*comparison ? "true" : "false", {}, comparison, false};
            return {text, {}, {}, false};
        }
        if (node.kind == "VectorLiteral" || node.kind == "MeasurementRecordLiteral" ||
            node.kind == "BitConcatenation") {
            const std::string prefix = node.kind == "MeasurementRecordLiteral" ? "mr[" :
                                       node.kind == "BitConcatenation" ? "{" : "[";
            const std::string suffix = node.kind == "BitConcatenation" ? "}" : "]";
            std::string text = prefix;
            bool first = true;
            for (const auto child : children(id)) {
                if (!first) text += ", ";
                first = false;
                text += evaluate(child, state).text;
            }
            return {text + suffix, {}, {}, false};
        }
        return {node.kind + "(" + node.value + ")", {}, {}, false};
    }

    bool constrain(ExecutionState& state, const SymbolicValue& condition,
                   bool expected) const {
        if (condition.boolean) return *condition.boolean == expected;
        for (const auto& existing : state.path.constraints) {
            if (existing.expression == condition.text) {
                return existing.expected == expected;
            }
        }
        state.path.constraints.push_back({condition.text, expected});
        return true;
    }

    std::vector<ExecutionState> fork_condition(ExecutionState state, NodeId condition_id,
                                                bool expected) {
        const auto& node = syntax(condition_id);
        if (node.kind == "Binary" && (node.value == "and" || node.value == "or")) {
            const auto lhs = required_edge(condition_id, "lhs");
            const auto rhs = required_edge(condition_id, "rhs");
            std::vector<ExecutionState> result;
            if ((node.value == "and" && expected) ||
                (node.value == "or" && !expected)) {
                auto first = fork_condition(std::move(state), lhs, expected);
                for (auto& first_state : first) {
                    auto second = fork_condition(std::move(first_state), rhs, expected);
                    result.insert(result.end(), std::make_move_iterator(second.begin()),
                                  std::make_move_iterator(second.end()));
                }
            } else {
                // `A and B == false` and `A or B == true` each describe one
                // control-flow choice but multiple concrete witnesses. Keep
                // them as a single symbolic constraint instead of duplicating
                // identical control paths for every witness.
                const auto lhs_value = evaluate(lhs, state);
                const auto rhs_value = evaluate(rhs, state);
                if (node.value == "and") {
                    if (lhs_value.boolean && !*lhs_value.boolean) {
                        result.push_back(std::move(state));
                    } else if (lhs_value.boolean && *lhs_value.boolean) {
                        result = fork_condition(std::move(state), rhs, false);
                    } else if (rhs_value.boolean && *rhs_value.boolean) {
                        result = fork_condition(std::move(state), lhs, false);
                    } else if (rhs_value.boolean && !*rhs_value.boolean) {
                        result.push_back(std::move(state));
                    }
                } else {
                    if (lhs_value.boolean && *lhs_value.boolean) {
                        result.push_back(std::move(state));
                    } else if (lhs_value.boolean && !*lhs_value.boolean) {
                        result = fork_condition(std::move(state), rhs, true);
                    } else if (rhs_value.boolean && !*rhs_value.boolean) {
                        result = fork_condition(std::move(state), lhs, true);
                    } else if (rhs_value.boolean && *rhs_value.boolean) {
                        result.push_back(std::move(state));
                    }
                }
                if (result.empty() && !lhs_value.boolean && !rhs_value.boolean) {
                    const auto compound = evaluate(condition_id, state);
                    if (constrain(state, compound, expected)) result.push_back(std::move(state));
                }
            }
            cap(result);
            return result;
        }

        bool normalized_expected = expected;
        SymbolicValue condition = evaluate(condition_id, state);
        if (node.kind == "Binary" && (node.value == "==" || node.value == "!=")) {
            const auto lhs = evaluate(required_edge(condition_id, "lhs"), state);
            const auto rhs = evaluate(required_edge(condition_id, "rhs"), state);
            condition = {"(" + lhs.text + ") == (" + rhs.text + ")", {}, {}, false};
            if (node.value == "!=") normalized_expected = !normalized_expected;
            if ((lhs.is_null || rhs.is_null) || (lhs.integer && rhs.integer) ||
                (lhs.boolean && rhs.boolean) || lhs.text == rhs.text) {
                condition = evaluate(condition_id, state);
                normalized_expected = expected;
            }
        }
        if (!constrain(state, condition, normalized_expected)) return {};
        return {std::move(state)};
    }

    void set_error(ExecutionState& state, std::string message) const {
        state.signal = Signal::Error;
        state.path.assertion_error = std::move(message);
        state.path.terminated = false;
    }

    std::vector<ExecutionState> execute_block(NodeId block,
                                               std::vector<ExecutionState> states) {
        for (const auto statement : children(block)) {
            std::vector<ExecutionState> next;
            for (auto& state : states) {
                if (state.signal == Signal::Normal) {
                    auto expanded = execute_statement(statement, std::move(state));
                    next.insert(next.end(), std::make_move_iterator(expanded.begin()),
                                std::make_move_iterator(expanded.end()));
                } else {
                    next.push_back(std::move(state));
                }
            }
            cap(next);
            states = std::move(next);
        }
        return states;
    }

    void execute_declaration(NodeId id, ExecutionState& state) const {
        const auto& node = syntax(id);
        if (const auto initializer = optional_edge(id, "initializer")) {
            state.values[node.value] = evaluate(*initializer, state);
            if (syntax(*initializer).kind == "VectorLiteral") {
                std::vector<SymbolicValue> values;
                for (const auto child : children(*initializer)) values.push_back(evaluate(child, state));
                state.vectors[node.value] = std::move(values);
            }
        } else {
            state.values[node.value] = {"⊥", {}, {}, true};
        }
    }

    std::string resolve_callee(NodeId call, const ExecutionState& state) const {
        const auto callee = required_edge(call, "callee");
        return evaluate(callee, state).text;
    }

    std::vector<ExecutionState> execute_tuple(NodeId id, ExecutionState state) {
        const auto targets = children(required_edge(id, "targets"));
        const auto call = required_edge(id, "call");
        const std::string se_name = resolve_callee(call, state);
        ++state.invocation;
        const std::string stem = "$r" + std::to_string(state.round) + "_" +
                                 state.phase + "_e" + std::to_string(state.invocation);
        const std::string syndrome = stem + ".s";
        std::optional<std::string> flag;
        if (targets.size() == 2) flag = stem + ".f";
        state.path.events.push_back({state.round, state.invocation, state.phase,
                                     se_name, syndrome, flag});
        if (!targets.empty()) state.values[syntax(targets[0]).value] = {syndrome, {}, {}, false};
        if (targets.size() == 2) state.values[syntax(targets[1]).value] = {*flag, {}, {}, false};
        return {std::move(state)};
    }

    std::vector<ExecutionState> execute_if(NodeId id, ExecutionState state) {
        const auto condition = required_edge(id, "condition");
        std::vector<ExecutionState> result;
        auto yes_states = fork_condition(state, condition, true);
        for (auto& yes : yes_states) {
            auto branch = execute_block(required_edge(id, "then"), {std::move(yes)});
            result.insert(result.end(), std::make_move_iterator(branch.begin()),
                          std::make_move_iterator(branch.end()));
        }
        auto no_states = fork_condition(std::move(state), condition, false);
        for (auto& no : no_states) {
            if (const auto alternative = optional_edge(id, "else")) {
                if (syntax(*alternative).kind == "If") {
                    auto branch = execute_if(*alternative, std::move(no));
                    result.insert(result.end(), std::make_move_iterator(branch.begin()),
                                  std::make_move_iterator(branch.end()));
                } else {
                    auto branch = execute_block(*alternative, {std::move(no)});
                    result.insert(result.end(), std::make_move_iterator(branch.begin()),
                                  std::make_move_iterator(branch.end()));
                }
            } else {
                result.push_back(std::move(no));
            }
        }
        cap(result);
        return result;
    }

    std::vector<ExecutionState> execute_switch(NodeId id, ExecutionState state) {
        const auto selector = evaluate(required_edge(id, "selector"), state);
        std::vector<std::pair<NodeId, NodeId>> cases;
        std::optional<NodeId> default_block;
        for (const auto& edge : syntax(id).edges) {
            if (edge.label == "default") default_block = edge.target;
            else if (edge.label.starts_with("case")) {
                cases.push_back({required_edge(edge.target, "value"),
                                 required_edge(edge.target, "body")});
            }
        }

        if (selector.integer) {
            for (const auto& [value_id, body] : cases) {
                const auto value = evaluate(value_id, state);
                if (value.integer && *value.integer == *selector.integer) {
                    return execute_block(body, {std::move(state)});
                }
            }
            if (default_block) return execute_block(*default_block, {std::move(state)});
            set_error(state, "switch has no matching case and no default");
            return {std::move(state)};
        }

        std::vector<ExecutionState> result;
        for (const auto& [value_id, body] : cases) {
            ExecutionState branch = state;
            const auto value = evaluate(value_id, branch);
            SymbolicValue equality{"(" + selector.text + ") == (" + value.text + ")", {}, {}, false};
            if (constrain(branch, equality, true)) {
                auto expanded = execute_block(body, {std::move(branch)});
                result.insert(result.end(), std::make_move_iterator(expanded.begin()),
                              std::make_move_iterator(expanded.end()));
            }
        }
        ExecutionState fallback = std::move(state);
        bool feasible = true;
        for (const auto& [value_id, body] : cases) {
            (void)body;
            const auto value = evaluate(value_id, fallback);
            SymbolicValue equality{"(" + selector.text + ") == (" + value.text + ")", {}, {}, false};
            feasible = feasible && constrain(fallback, equality, false);
        }
        if (feasible) {
            if (default_block) {
                auto expanded = execute_block(*default_block, {std::move(fallback)});
                result.insert(result.end(), std::make_move_iterator(expanded.begin()),
                              std::make_move_iterator(expanded.end()));
            } else {
                set_error(fallback, "switch has no matching case and no default");
                result.push_back(std::move(fallback));
            }
        }
        cap(result);
        return result;
    }

    std::vector<ExecutionState> execute_while(NodeId id, ExecutionState state) {
        const auto condition = required_edge(id, "condition");
        std::vector<ExecutionState> result;
        std::vector<ExecutionState> pending{std::move(state)};
        while (!pending.empty()) {
            std::vector<ExecutionState> next_iteration;
            for (auto& current_state : pending) {
                if (!consume(current_state)) {
                    result.push_back(std::move(current_state));
                    continue;
                }
                auto exit_states = fork_condition(current_state, condition, false);
                result.insert(result.end(), std::make_move_iterator(exit_states.begin()),
                              std::make_move_iterator(exit_states.end()));

                auto enter_states = fork_condition(std::move(current_state), condition, true);
                for (auto& enter : enter_states) {
                    auto body_states = execute_block(required_edge(id, "body"), {std::move(enter)});
                    for (auto& body_state : body_states) {
                        if (body_state.signal == Signal::Break) {
                            body_state.signal = Signal::Normal;
                            result.push_back(std::move(body_state));
                        } else if (body_state.signal == Signal::Normal ||
                                   body_state.signal == Signal::Continue) {
                            body_state.signal = Signal::Normal;
                            next_iteration.push_back(std::move(body_state));
                        } else {
                            result.push_back(std::move(body_state));
                        }
                    }
                }
            }
            cap(result);
            cap(next_iteration);
            pending = std::move(next_iteration);
        }
        return result;
    }

    std::vector<ExecutionState> branch_terminal_conditions(ExecutionState state) {
        std::vector<ExecutionState> result;
        for (std::size_t selected = 0; selected < terminal_conditions_.size(); ++selected) {
            std::vector<ExecutionState> candidates{state};
            for (std::size_t i = 0; i < terminal_conditions_.size(); ++i) {
                std::vector<ExecutionState> constrained;
                for (auto& candidate : candidates) {
                    auto expanded = fork_condition(std::move(candidate),
                        required_edge(terminal_conditions_[i], "condition"), i == selected);
                    constrained.insert(constrained.end(), std::make_move_iterator(expanded.begin()),
                                       std::make_move_iterator(expanded.end()));
                }
                cap(constrained);
                candidates = std::move(constrained);
                if (candidates.empty()) break;
            }
            for (auto& terminal : candidates) {
                const int id = std::stoi(syntax(terminal_conditions_[selected]).value);
                terminal.path.terminal_condition = id;
                terminal.path.terminating_policy = id;
                terminal.phase = "tp";
                terminal.invocation = 0;
                const auto policy = policies_.find(id);
                if (policy == policies_.end()) {
                    set_error(terminal, "missing terminating policy " + std::to_string(id));
                    result.push_back(std::move(terminal));
                    continue;
                }
                auto policy_states = execute_block(policy->second, {std::move(terminal)});
                for (auto& policy_state : policy_states) {
                    if (policy_state.signal == Signal::Normal) {
                        if (consume(policy_state)) {
                            policy_state.signal = Signal::Terminated;
                            policy_state.path.terminated = true;
                            policy_state.path.terminal_action = "end";
                        }
                    } else if (policy_state.signal == Signal::Break ||
                               policy_state.signal == Signal::Continue) {
                        set_error(policy_state, "break/continue escaped a terminating policy");
                    }
                    result.push_back(std::move(policy_state));
                }
            }
        }

        std::vector<ExecutionState> continuations{std::move(state)};
        for (const auto tc : terminal_conditions_) {
            std::vector<ExecutionState> constrained;
            for (auto& continuation : continuations) {
                auto expanded = fork_condition(std::move(continuation),
                    required_edge(tc, "condition"), false);
                constrained.insert(constrained.end(), std::make_move_iterator(expanded.begin()),
                                   std::make_move_iterator(expanded.end()));
            }
            cap(constrained);
            continuations = std::move(constrained);
            if (continuations.empty()) break;
        }
        result.insert(result.end(), std::make_move_iterator(continuations.begin()),
                      std::make_move_iterator(continuations.end()));
        cap(result);
        return result;
    }

    std::vector<ExecutionState> execute_statement(NodeId id, ExecutionState state) {
        if (!consume(state)) return {std::move(state)};
        const auto& node = syntax(id);
        if (node.kind == "Declaration") {
            execute_declaration(id, state);
            return {std::move(state)};
        }
        if (node.kind == "Assign") {
            const auto lhs = required_edge(id, "lhs");
            state.values[syntax(lhs).value] = evaluate(required_edge(id, "rhs"), state);
            return {std::move(state)};
        }
        if (node.kind == "Increment" || node.kind == "AddAssign") {
            const auto target = required_edge(id, node.kind == "Increment" ? "target" : "lhs");
            const std::string name = syntax(target).value;
            const auto current_value = state.values.contains(name) ? state.values.at(name) : SymbolicValue{name, {}, {}, false};
            const auto amount = node.kind == "Increment" ? SymbolicValue{"1", 1, {}, false}
                                                          : evaluate(required_edge(id, "rhs"), state);
            if (current_value.integer && amount.integer) {
                const auto value = *current_value.integer + *amount.integer;
                state.values[name] = {std::to_string(value), value, {}, false};
            } else {
                state.values[name] = {parenthesize(current_value) + " + " + parenthesize(amount), {}, {}, false};
            }
            return {std::move(state)};
        }
        if (node.kind == "Push") {
            const auto target = required_edge(id, "target");
            const std::string name = syntax(target).value;
            const auto value = evaluate(required_edge(id, "value"), state);
            const auto current_value = state.values.contains(name) ? state.values.at(name) : SymbolicValue{name, {}, {}, false};
            state.values[name] = {"push(" + current_value.text + ", " + value.text + ")", {}, {}, false};
            return {std::move(state)};
        }
        if (node.kind == "SETupleAssignment") return execute_tuple(id, std::move(state));
        if (node.kind == "If") return execute_if(id, std::move(state));
        if (node.kind == "Switch") return execute_switch(id, std::move(state));
        if (node.kind == "While") return execute_while(id, std::move(state));
        if (node.kind == "CheckTerminalConditions") return branch_terminal_conditions(std::move(state));
        if (node.kind == "Break") {
            state.signal = Signal::Break;
            return {std::move(state)};
        }
        if (node.kind == "Continue") {
            state.signal = Signal::Continue;
            return {std::move(state)};
        }
        if (node.kind == "Decode") {
            state.signal = Signal::Terminated;
            state.path.terminated = true;
            state.path.terminal_action = "decode";
            state.path.decode_record = evaluate(required_edge(id, "record"), state).text;
            return {std::move(state)};
        }
        if (node.kind == "End") {
            state.signal = Signal::Terminated;
            state.path.terminated = true;
            state.path.terminal_action = "end";
            return {std::move(state)};
        }
        set_error(state, "unsupported statement kind " + node.kind);
        return {std::move(state)};
    }

    std::vector<SymbolicPath> expand_paths(NodeId root) {
        const auto globals = required_edge(root, "gvar");
        const auto tc_section = required_edge(root, "tc");
        const auto af = required_edge(root, "af");
        const auto tp_section = required_edge(root, "tp");
        terminal_conditions_ = children(tc_section);
        for (const auto policy : children(tp_section)) {
            policies_[std::stoi(syntax(policy).value)] = required_edge(policy, "body");
        }

        ExecutionState initial;
        for (const auto declaration : children(globals)) execute_declaration(declaration, initial);

        std::vector<ExecutionState> frontier{std::move(initial)};
        std::vector<ExecutionState> completed;
        const auto af_body = required_edge(af, "body");
        while (!frontier.empty()) {
            std::vector<ExecutionState> next_round;
            for (auto& state : frontier) {
                if (!consume(state)) {
                    completed.push_back(std::move(state));
                    continue;
                }
                auto checked = branch_terminal_conditions(std::move(state));
                for (auto& checked_state : checked) {
                    if (checked_state.path.terminal_condition ||
                        checked_state.signal != Signal::Normal) {
                        completed.push_back(std::move(checked_state));
                        continue;
                    }
                    ++checked_state.round;
                    checked_state.path.rounds = checked_state.round;
                    checked_state.invocation = 0;
                    checked_state.phase = "af";
                    auto body_states = execute_block(af_body, {std::move(checked_state)});
                    for (auto& body_state : body_states) {
                        if (body_state.signal == Signal::Normal ||
                            body_state.signal == Signal::Continue) {
                            body_state.signal = Signal::Normal;
                            next_round.push_back(std::move(body_state));
                        } else if (body_state.signal == Signal::Break) {
                            set_error(body_state, "break exited adaptive flow before a terminal condition");
                            completed.push_back(std::move(body_state));
                        } else {
                            completed.push_back(std::move(body_state));
                        }
                    }
                }
            }
            cap(next_round);
            cap(completed);
            frontier = std::move(next_round);
        }

        if (truncated_) {
            diagnostics_.push_back({Diagnostic::Severity::Warning, syntax(root).location,
                "symbolic path expansion reached max_paths; output is truncated"});
        }
        std::vector<SymbolicPath> paths;
        paths.reserve(completed.size());
        for (auto& state : completed) paths.push_back(std::move(state.path));
        cap(paths);
        return paths;
    }

    std::vector<Token> tokens_;
    std::size_t position_ = 0;
    std::filesystem::path source_path_;
    ParseOptions options_;
    SyntaxTree ast_;
    std::vector<Diagnostic> diagnostics_;
    std::set<std::string> se_names_;
    std::unordered_map<std::string, std::size_t> se_arity_;
    std::set<std::string> global_names_;
    std::set<std::string> tc_ids_;
    std::set<std::string> tp_ids_;
    bool parsing_globals_ = false;
    bool parsing_tc_ = false;
    bool in_tp_ = false;
    std::size_t loop_depth_ = 0;
    std::size_t switch_depth_ = 0;
    bool truncated_ = false;
    std::vector<NodeId> terminal_conditions_;
    std::unordered_map<int, NodeId> policies_;
};

} // namespace

std::string ParseResult::to_json() const {
    std::ostringstream out;
    out << "{\n  \"protocol\": \"" << json_escape(protocol_name)
        << "\",\n  \"truncated\": " << (truncated ? "true" : "false")
        << ",\n  \"path_count\": " << paths.size() << ",\n  \"paths\": [\n";
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const auto& path = paths[i];
        out << "    {\"id\": " << path.id
            << ", \"rounds\": " << path.rounds
            << ", \"transitions\": " << path.transitions
            << ", \"terminated\": " << (path.terminated ? "true" : "false")
            << ", \"bound_exceeded\": " << (path.bound_exceeded ? "true" : "false")
            << ", \"tc\": ";
        if (path.terminal_condition) out << *path.terminal_condition; else out << "null";
        out << ", \"tp\": ";
        if (path.terminating_policy) out << *path.terminating_policy; else out << "null";
        out << ", \"terminal_action\": \"" << json_escape(path.terminal_action) << "\"";
        out << ", \"decode_record\": ";
        if (path.decode_record) out << '"' << json_escape(*path.decode_record) << '"'; else out << "null";
        out << ", \"assertion_error\": ";
        if (path.assertion_error) out << '"' << json_escape(*path.assertion_error) << '"'; else out << "null";
        out << ", \"events\": [";
        for (std::size_t j = 0; j < path.events.size(); ++j) {
            const auto& event = path.events[j];
            if (j) out << ", ";
            out << "{\"round\": " << event.round
                << ", \"invocation\": " << event.invocation
                << ", \"phase\": \"" << json_escape(event.phase)
                << "\", \"se\": \"" << json_escape(event.se_name)
                << "\", \"s\": \"" << json_escape(event.syndrome)
                << "\", \"f\": ";
            if (event.flag) out << '"' << json_escape(*event.flag) << '"'; else out << "null";
            out << '}';
        }
        out << "], \"constraints\": [";
        for (std::size_t j = 0; j < path.constraints.size(); ++j) {
            if (j) out << ", ";
            out << "{\"expression\": \"" << json_escape(path.constraints[j].expression)
                << "\", \"expected\": " << (path.constraints[j].expected ? "true" : "false") << '}';
        }
        out << "]}";
        if (i + 1 != paths.size()) out << ',';
        out << '\n';
    }
    out << "  ]\n}";
    return out.str();
}

ParseError::ParseError(SourceLocation location, std::string message)
    : std::runtime_error(std::move(message)), location_(location) {}

SourceLocation ParseError::location() const noexcept { return location_; }

ParseResult Parser::parse_file(const std::filesystem::path& path,
                               const ParseOptions& options) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open FPDL file '" + path.string() + "'");
    const std::string source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    return parse_string(source, path, options);
}

ParseResult Parser::parse_string(std::string_view source,
                                 const std::filesystem::path& source_path,
                                 const ParseOptions& options) {
    return ParserImpl(source, source_path, options).run();
}

} // namespace fpdl
