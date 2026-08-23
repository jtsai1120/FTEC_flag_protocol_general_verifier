#include "qasm_propagate.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace pbdd {

namespace {

// ---------------------------------------------------------------------------
// Lexing: strip comments, split on ';', remember where each statement began so
// errors can point at a line.
// ---------------------------------------------------------------------------

struct Stmt {
    std::string text;
    int         line;
};

std::string trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

[[noreturn]] void fail(int line, const std::string &msg) {
    std::ostringstream os;
    os << "QASM line " << line << ": " << msg;
    throw std::runtime_error(os.str());
}

std::vector<Stmt> split_statements(const std::string &src) {
    std::vector<Stmt> out;
    std::string cur;
    int  line = 1;
    int  start_line = 1;
    bool started = false;

    for (size_t i = 0; i < src.size();) {
        const char c = src[i];

        if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            const int open_line = line;
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') ++line;
                ++i;
            }
            if (i + 1 >= src.size()) fail(open_line, "unterminated /* comment");
            i += 2;
            continue;
        }

        if (c == '\n') ++line;

        if (c == ';') {
            const std::string t = trim(cur);
            if (!t.empty()) out.push_back({t, start_line});
            cur.clear();
            started = false;
            ++i;
            continue;
        }

        if (!started && !std::isspace(static_cast<unsigned char>(c))) {
            started = true;
            start_line = line;
        }
        cur += c;
        ++i;
    }

    if (!trim(cur).empty()) fail(start_line, "statement is not terminated by ';'");
    return out;
}

// ---------------------------------------------------------------------------
// Parsing: the accepted subset of OpenQASM 3.
// ---------------------------------------------------------------------------

enum class GateKind { X, Z, H, CX, CY, CZ };

struct Gate {
    GateKind kind;
    int      q0;
    int      q1;  // only meaningful for CX / CZ
};

bool is_two_qubit(GateKind k) {
    return k == GateKind::CX || k == GateKind::CY || k == GateKind::CZ;
}

// The two registers are flattened data-first: qd[i] -> i, qm[j] -> nd + j.
struct Circuit {
    int               nd = 0;  // data qubits
    int               nm = 0;  // measurement qubits
    std::vector<Gate> gates;

    int n_qubits() const { return nd + nm; }
};

Circuit parse(const std::string &src) {
    static const std::regex re_version(R"(^OPENQASM\s+3(\.\d+)?$)");
    // Custom delimiter: the pattern itself contains the )" sequence.
    static const std::regex re_include(R"rx(^include\s+"([^"]*)"$)rx");
    static const std::regex re_qubit(R"(^qubit\s*\[\s*(\d+)\s*\]\s+([A-Za-z_][A-Za-z0-9_]*)$)");
    static const std::regex re_barrier(R"(^barrier(\s.*)?$)");
    static const std::regex re_gate1(
        R"(^(x|z|h)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*(\d+)\s*\]$)");
    static const std::regex re_gate2(
        R"(^(cx|cy|cz)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*(\d+)\s*\]\s*,\s*)"
        R"(([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*(\d+)\s*\]$)");

    const std::vector<Stmt> stmts = split_statements(src);
    if (stmts.empty()) throw std::runtime_error("QASM: file contains no statements");

    Circuit     circuit;
    bool        seen_version = false;
    bool        seen_qd      = false;
    bool        seen_qm      = false;
    std::smatch m;

    auto have_registers = [&] { return seen_qd && seen_qm; };

    for (const Stmt &s : stmts) {
        if (std::regex_match(s.text, m, re_version)) {
            if (seen_version) fail(s.line, "duplicate OPENQASM header");
            seen_version = true;
            continue;
        }
        if (!seen_version) {
            fail(s.line, "expected an 'OPENQASM 3;' header before any other statement, got: "
                             + s.text);
        }

        if (std::regex_match(s.text, m, re_include)) {
            if (m[1].str() != "stdgates.inc") {
                fail(s.line, "only include \"stdgates.inc\" is supported, got: " + m[1].str());
            }
            continue;
        }

        if (std::regex_match(s.text, m, re_qubit)) {
            const int         n    = std::stoi(m[1].str());
            const std::string name = m[2].str();
            if (n <= 0) fail(s.line, "qubit register must have a positive size");
            if (name == "qd") {
                if (seen_qd) fail(s.line, "duplicate qd register");
                circuit.nd = n;
                seen_qd    = true;
            } else if (name == "qm") {
                if (seen_qm) fail(s.line, "duplicate qm register");
                circuit.nm = n;
                seen_qm    = true;
            } else {
                fail(s.line, "qubit registers must be named 'qd' (data) or 'qm' "
                             "(measurement), got: " + name);
            }
            continue;
        }

        if (std::regex_match(s.text, m, re_barrier)) continue;  // scheduling hint only

        // Everything below addresses qubits, so the register must exist first.
        const bool one = std::regex_match(s.text, m, re_gate1);
        const bool two = one ? false : std::regex_match(s.text, m, re_gate2);
        if (!one && !two) {
            fail(s.line, "unsupported statement (only x/z/h/cx/cy/cz on explicitly "
                         "indexed qubits are allowed): " + s.text);
        }
        if (!have_registers()) {
            fail(s.line, "gate appears before both qd and qm have been declared");
        }

        // Flatten to the global index space: qd[i] -> i, qm[j] -> nd + j.
        auto qubit_index = [&](const std::string &name, const std::string &idx) {
            const int i     = std::stoi(idx);
            const int limit = (name == "qd") ? circuit.nd : circuit.nm;
            if (name != "qd" && name != "qm") {
                fail(s.line, "unknown qubit register '" + name + "' (expected qd or qm)");
            }
            if (i >= limit) {
                fail(s.line, "qubit index " + idx + " is out of range for " + name + "["
                                 + std::to_string(limit) + "]");
            }
            return (name == "qd") ? i : circuit.nd + i;
        };

        Gate g{};
        if (one) {
            const std::string name = m[1].str();
            g.kind = (name == "x") ? GateKind::X : (name == "z") ? GateKind::Z : GateKind::H;
            g.q0   = qubit_index(m[2].str(), m[3].str());
            g.q1   = -1;
        } else {
            const std::string name = m[1].str();
            g.kind = (name == "cx") ? GateKind::CX
                   : (name == "cy") ? GateKind::CY
                                    : GateKind::CZ;
            g.q0   = qubit_index(m[2].str(), m[3].str());
            g.q1   = qubit_index(m[4].str(), m[5].str());
            if (g.q0 == g.q1) fail(s.line, "a two-qubit gate needs two distinct qubits");
        }
        circuit.gates.push_back(g);
    }

    if (!seen_qd) throw std::runtime_error("QASM: no data register 'qd' declared");
    if (!seen_qm) throw std::runtime_error("QASM: no measurement register 'qm' declared");
    return circuit;
}

// Run every tic of `circuit` over a t-indexed state, in place. Returns the
// number of fault locations (two-qubit gates) encountered.
int run_tics(std::vector<PauliSetBDD> &by_t, const Circuit &circuit, int tau);

PauliSetBDD apply_gate(const PauliSetBDD &s, const Gate &g) {
    switch (g.kind) {
        case GateKind::X:  return s.apply_X(g.q0);   // composition (Pauli frame)
        case GateKind::Z:  return s.apply_Z(g.q0);   // composition (Pauli frame)
        case GateKind::H:  return s.apply_H(g.q0);   // conjugation
        case GateKind::CX: return s.apply_CX(g.q0, g.q1);
        case GateKind::CY: return s.apply_CY(g.q0, g.q1);
        case GateKind::CZ: return s.apply_CZ(g.q0, g.q1);
    }
    throw std::logic_error("apply_gate: unreachable");
}

int run_tics(std::vector<PauliSetBDD> &by_t, const Circuit &circuit, int tau) {
    int fault_locations = 0;

    for (const Gate &g : circuit.gates) {
        // (A) the gate acts on every fault count.
        for (PauliSetBDD &s : by_t) s = apply_gate(s, g);

        if (!is_two_qubit(g.kind)) continue;  // single-qubit gates never fault
        ++fault_locations;

        // (B) spawn from the post-gate state -- every spawn reads by_t before
        // any of them is merged, which is what keeps a tic to one fault per
        // lineage. spawn[t] is destined for t+1.
        std::vector<PauliSetBDD> spawn;
        spawn.reserve(static_cast<size_t>(tau));
        for (int t = 0; t < tau; ++t) {
            spawn.push_back(by_t[static_cast<size_t>(t)].multiply_by_all_paulis_on(g.q0, g.q1));
        }

        // (C) merge into whatever already sits one fault higher.
        for (int t = 0; t < tau; ++t) {
            by_t[static_cast<size_t>(t) + 1] =
                by_t[static_cast<size_t>(t) + 1] | spawn[static_cast<size_t>(t)];
        }
    }
    return fault_locations;
}

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open QASM file: " + path);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

} // namespace

// ---------------------------------------------------------------------------
// QasmPropagation: owns the BuDDy session.
// ---------------------------------------------------------------------------

void QasmPropagation::release() noexcept {
    if (!owns_session_) return;
    owns_session_ = false;
    try {
        // Every bdd destructor calls bdd_delref, so all of them must run while
        // the package is still alive -- clearing before done() is required, not
        // just tidy.
        branches_.clear();
        by_t_.clear();
        PauliSetBDD::done();
    } catch (...) {
        // Nothing useful to do from a destructor.
    }
}

QasmPropagation::~QasmPropagation() { release(); }

QasmPropagation::QasmPropagation(QasmPropagation &&other) noexcept
    : n_data_(other.n_data_),
      n_measure_(other.n_measure_),
      tau_(other.tau_),
      n_tics_(other.n_tics_),
      n_faults_(other.n_faults_),
      owns_session_(other.owns_session_),
      by_t_(std::move(other.by_t_)),
      branches_(std::move(other.branches_)) {
    other.owns_session_ = false;
}

QasmPropagation &QasmPropagation::operator=(QasmPropagation &&other) noexcept {
    if (this != &other) {
        release();
        n_data_       = other.n_data_;
        n_measure_    = other.n_measure_;
        tau_          = other.tau_;
        n_tics_       = other.n_tics_;
        n_faults_     = other.n_faults_;
        owns_session_ = other.owns_session_;
        by_t_         = std::move(other.by_t_);
        branches_     = std::move(other.branches_);
        other.owns_session_ = false;
    }
    return *this;
}

const PauliSetBDD &QasmPropagation::at(int t) const {
    if (t < 0 || t > tau_) throw std::out_of_range("QasmPropagation::at: t out of range");
    return by_t_[static_cast<size_t>(t)];
}

int QasmPropagation::data_qubit(int i) const {
    if (i < 0 || i >= n_data_) throw std::out_of_range("QasmPropagation::data_qubit");
    return i;
}

int QasmPropagation::measure_qubit(int j) const {
    if (j < 0 || j >= n_measure_) throw std::out_of_range("QasmPropagation::measure_qubit");
    return n_data_ + j;
}

namespace {

// Split `s` on the syndrome bits of qm[j..nm), appending the non-empty leaves.
//
// Measurement is in the Z basis, so qm[j]'s outcome flips exactly when the
// Pauli on it anticommutes with Z, i.e. when its x component is set. Splitting
// on x_qm[j] therefore separates {I,Z} (bit 0) from {X,Y} (bit 1).
//
// Intersecting rather than restricting keeps each branch an exact subset of
// `s`, so the branches at one t partition it. Recursing depth-first prunes an
// empty prefix immediately instead of enumerating all 2^nm patterns under it.
void split_by_syndrome(const PauliSetBDD &s, int j, int nm, int n_data, int t,
                       std::string &mr, std::vector<SyndromeBranch> &out) {
    if (s.is_empty()) return;
    if (j == nm) {
        out.push_back(SyndromeBranch{t, mr, s});
        return;
    }

    const int v = PauliSetBDD::xvar(n_data + j);

    mr.push_back('0');
    split_by_syndrome(s & PauliSetBDD(bdd_nithvar(v)), j + 1, nm, n_data, t, mr, out);
    mr.back() = '1';
    split_by_syndrome(s & PauliSetBDD(bdd_ithvar(v)), j + 1, nm, n_data, t, mr, out);
    mr.pop_back();
}

} // namespace

namespace {

// The measurement qubits of a round, as global indices.
std::vector<int> measure_qubits(int nd, int nm) {
    std::vector<int> qs;
    qs.reserve(static_cast<size_t>(nm));
    for (int j = 0; j < nm; ++j) qs.push_back(nd + j);
    return qs;
}

} // namespace

QasmPropagation propagate_qasm(const std::string &path, int tau, bool reset_measure) {
    if (tau < 0) throw std::invalid_argument("propagate_qasm: tau must be >= 0");

    const Circuit circuit = parse(read_file(path));

    PauliSetBDD::init(circuit.n_qubits());

    // From here on `result` owns the session: if anything below throws, its
    // destructor calls done() exactly once.
    QasmPropagation result;
    result.owns_session_ = true;
    result.n_data_       = circuit.nd;
    result.n_measure_    = circuit.nm;
    result.tau_          = tau;
    result.n_tics_       = static_cast<int>(circuit.gates.size());

    result.by_t_.reserve(static_cast<size_t>(tau) + 1);
    result.by_t_.push_back(PauliSetBDD::single(std::string(circuit.n_qubits(), 'I')));
    for (int t = 1; t <= tau; ++t) result.by_t_.push_back(PauliSetBDD::empty());

    result.n_faults_ = run_tics(result.by_t_, circuit, tau);

    // After the last tic, split each fault level by the syndrome its
    // measurement qubits carry. No reordering is needed: the intersections
    // are independent of the variable order.
    std::string mr;
    mr.reserve(static_cast<size_t>(circuit.nm));
    for (int t = 0; t <= tau; ++t) {
        split_by_syndrome(result.by_t_[static_cast<size_t>(t)], 0, circuit.nm, circuit.nd, t,
                          mr, result.branches_);
    }

    if (reset_measure) {
        const std::vector<int> qm = measure_qubits(circuit.nd, circuit.nm);
        for (SyndromeBranch &b : result.branches_) b.set = b.set.reset_qubits(qm);
    }

    return result;
}

// ---------------------------------------------------------------------------
// PauliFlow: a chain of circuits over one session and one fault budget.
// ---------------------------------------------------------------------------

PauliFlow::PauliFlow(int tau) : tau_(tau) {
    if (tau < 0) throw std::invalid_argument("PauliFlow: tau must be >= 0");
}

void PauliFlow::release() noexcept {
    if (!owns_session_) return;
    owns_session_ = false;
    try {
        // Same ordering rule as QasmPropagation: every bdd must be destroyed
        // while the package is still alive.
        branches_.clear();
        PauliSetBDD::done();
    } catch (...) {
        // Nothing useful to do from a destructor.
    }
}

PauliFlow::~PauliFlow() { release(); }

PauliFlow::PauliFlow(PauliFlow &&other) noexcept
    : tau_(other.tau_),
      n_rounds_(other.n_rounds_),
      n_data_(other.n_data_),
      n_measure_(other.n_measure_),
      owns_session_(other.owns_session_),
      branches_(std::move(other.branches_)) {
    other.owns_session_ = false;
}

PauliFlow &PauliFlow::operator=(PauliFlow &&other) noexcept {
    if (this != &other) {
        release();
        tau_          = other.tau_;
        n_rounds_     = other.n_rounds_;
        n_data_       = other.n_data_;
        n_measure_    = other.n_measure_;
        owns_session_ = other.owns_session_;
        branches_     = std::move(other.branches_);
        other.owns_session_ = false;
    }
    return *this;
}

void PauliFlow::run(const std::string &qasm_path, bool reset_measure) {
    const Circuit circuit = parse(read_file(qasm_path));

    if (n_rounds_ == 0) {
        n_data_    = circuit.nd;
        n_measure_ = circuit.nm;
        PauliSetBDD::init(n_qubits());
        owns_session_ = true;   // the destructor is responsible for done() now

        branches_.push_back(
            SyndromeBranch{0, std::string(), PauliSetBDD::single(std::string(n_qubits(), 'I'))});
    } else {
        if (circuit.nd != n_data_) {
            throw std::runtime_error("PauliFlow: every circuit in a chain must declare the "
                                     "same data register width (have qd[" +
                                     std::to_string(n_data_) + "], got qd[" +
                                     std::to_string(circuit.nd) + "])");
        }
        if (circuit.nm > n_measure_) {
            // Widen the ancilla space. The new variables are unconstrained in
            // the branches built so far, so pin them to identity at once --
            // otherwise every branch would silently gain all 4^added patterns.
            const int old_nm = n_measure_;
            n_measure_       = circuit.nm;
            PauliSetBDD::grow(n_qubits());

            std::vector<int> added;
            for (int j = old_nm; j < n_measure_; ++j) added.push_back(n_data_ + j);
            for (SyndromeBranch &b : branches_) b.set = b.set.reset_qubits(added);
        }
    }

    // Branches sharing a record form one t-indexed state, so they propagate
    // together and any two paths landing on the same (t, mr) merge on the way.
    std::map<std::string, std::vector<PauliSetBDD>> groups;
    for (const SyndromeBranch &b : branches_) {
        auto it = groups.find(b.mr);
        if (it == groups.end()) {
            std::vector<PauliSetBDD> level(static_cast<size_t>(tau_) + 1, PauliSetBDD::empty());
            it = groups.emplace(b.mr, std::move(level)).first;
        }
        std::vector<PauliSetBDD> &level = it->second;
        const size_t             t      = static_cast<size_t>(b.t);
        level[t] = level[t] | b.set;
    }

    const std::vector<int> qm = measure_qubits(n_data_, circuit.nm);

    std::vector<SyndromeBranch> next;
    for (auto &entry : groups) {
        std::vector<PauliSetBDD> &by_t = entry.second;
        run_tics(by_t, circuit, tau_);

        // Append this round's outcome to the record carried so far.
        std::string prefix = entry.first;
        if (!prefix.empty()) prefix.push_back(kRoundSeparator);

        for (int t = 0; t <= tau_; ++t) {
            std::string mr = prefix;
            split_by_syndrome(by_t[static_cast<size_t>(t)], 0, circuit.nm, n_data_, t, mr, next);
        }
    }

    if (reset_measure) {
        for (SyndromeBranch &b : next) b.set = b.set.reset_qubits(qm);
    }

    // split_by_syndrome walks records in order within a group and the groups
    // come from an ordered map, but t is the outer loop per group, so a final
    // sort is what actually puts the whole set in (t, mr) order.
    std::sort(next.begin(), next.end(),
              [](const SyndromeBranch &a, const SyndromeBranch &b) {
                  return a.t != b.t ? a.t < b.t : a.mr < b.mr;
              });

    branches_ = std::move(next);
    ++n_rounds_;
}

} // namespace pbdd
