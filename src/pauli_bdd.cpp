#include "pauli_bdd.hpp"

#include <cctype>
#include <stdexcept>

namespace pbdd {

namespace {

// Substitute variable v inside f with boolean function g (one traversal,
// memoized via BuDDy's operator cache). Named wrapper so call sites read
// as the linear substitution they encode rather than a raw bdd_compose.
bdd substitute(const bdd &f, int v, const bdd &g) { return bdd_compose(f, g, v); }

void check_qubit(int q, int n, const char *who) {
    if (q < 0 || q >= n) {
        throw std::invalid_argument(std::string(who) + ": qubit index out of range");
    }
}

} // namespace

int  PauliSetBDD::n_ = 0;
bool PauliSetBDD::initialized_ = false;

void PauliSetBDD::check_init() {
    if (!initialized_) {
        throw std::runtime_error("PauliSetBDD::init() must be called before use");
    }
}

void PauliSetBDD::init(int n_qubits, int node_num, int cache_size) {
    if (initialized_) {
        throw std::runtime_error("PauliSetBDD::init() called twice; call done() first");
    }
    if (n_qubits <= 0) {
        throw std::invalid_argument("n_qubits must be positive");
    }

    n_ = n_qubits;
    bdd_init(node_num, cache_size);
    bdd_setvarnum(2 * n_);

    // Declare every variable as its own reorder block, i.e. impose no
    // adjacency constraint at all -- let sifting find the best order
    // freely. The interleaved declaration order above (x0,z0,x1,z1,...)
    // is just a reasonable *starting point* for the sifting algorithm.
    bdd_varblockall();
    bdd_autoreorder(BDD_REORDER_SIFT);

    initialized_ = true;
}

void PauliSetBDD::grow(int n_qubits) {
    check_init();
    if (n_qubits < n_) {
        throw std::invalid_argument("PauliSetBDD::grow(): BuDDy cannot drop variables");
    }
    if (n_qubits == n_) return;

    const int old_n = n_;
    n_ = n_qubits;
    bdd_setvarnum(2 * n_);

    // bdd_setvarnum only resizes the quantification scratch space; it leaves
    // the operator caches alone. bdd_satcount's result depends on the total
    // variable count (it charges 2^gap for every level a path skips, including
    // the gap down to the terminal), so counts cached before the resize come
    // back stale and too small afterwards. bdd_gbc() runs bdd_operator_reset()
    // and clears them.
    bdd_gbc();

    // init() gave every variable its own reorder block via bdd_varblockall();
    // the ones added just now need the same treatment, or sifting would leave
    // them pinned. Block membership is by variable number, not level.
    for (int v = 2 * old_n; v < 2 * n_; ++v) {
        bdd_intaddvarblock(v, v, BDD_REORDER_FREE);
    }
}

void PauliSetBDD::done() {
    check_init();
    bdd_done();
    initialized_ = false;
    n_ = 0;
}

Pauli PauliSetBDD::char_to_pauli(char c) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'I': return Pauli::I;
        case 'X': return Pauli::X;
        case 'Z': return Pauli::Z;
        case 'Y': return Pauli::Y;
        default:
            throw std::invalid_argument(std::string("invalid Pauli character: ") + c);
    }
}

char PauliSetBDD::pauli_to_char(Pauli p) {
    switch (p) {
        case Pauli::I: return 'I';
        case Pauli::X: return 'X';
        case Pauli::Z: return 'Z';
        case Pauli::Y: return 'Y';
    }
    return '?';
}

PauliSetBDD PauliSetBDD::empty() {
    check_init();
    return PauliSetBDD(bddfalse);
}

PauliSetBDD PauliSetBDD::universe() {
    check_init();
    return PauliSetBDD(bddtrue);
}

PauliSetBDD PauliSetBDD::single(const std::vector<Pauli> &paulis) {
    check_init();
    if (static_cast<int>(paulis.size()) != n_) {
        throw std::invalid_argument("single(): paulis.size() must equal num_qubits()");
    }

    bdd cube = bddtrue;
    for (int q = 0; q < n_; ++q) {
        const uint8_t p = static_cast<uint8_t>(paulis[q]);
        const bool has_x = p & 0x1;
        const bool has_z = p & 0x2;
        cube &= has_x ? bdd_ithvar(xvar(q)) : bdd_nithvar(xvar(q));
        cube &= has_z ? bdd_ithvar(zvar(q)) : bdd_nithvar(zvar(q));
    }
    return PauliSetBDD(cube);
}

PauliSetBDD PauliSetBDD::single(const std::string &paulis) {
    std::vector<Pauli> v;
    v.reserve(paulis.size());
    for (char c : paulis) v.push_back(char_to_pauli(c));
    return single(v);
}

PauliSetBDD PauliSetBDD::operator|(const PauliSetBDD &rhs) const { return PauliSetBDD(f_ | rhs.f_); }
PauliSetBDD PauliSetBDD::operator&(const PauliSetBDD &rhs) const { return PauliSetBDD(f_ & rhs.f_); }
PauliSetBDD PauliSetBDD::operator-(const PauliSetBDD &rhs) const { return PauliSetBDD(f_ - rhs.f_); }
PauliSetBDD PauliSetBDD::operator!() const { return PauliSetBDD(!f_); }

bool PauliSetBDD::operator==(const PauliSetBDD &rhs) const { return f_ == rhs.f_; }
bool PauliSetBDD::operator!=(const PauliSetBDD &rhs) const { return f_ != rhs.f_; }

bool PauliSetBDD::contains(const std::vector<Pauli> &paulis) const {
    const PauliSetBDD point = single(paulis);
    return (f_ & point.f_) != bddfalse;
}

bool PauliSetBDD::contains(const std::string &paulis) const {
    std::vector<Pauli> v;
    v.reserve(paulis.size());
    for (char c : paulis) v.push_back(char_to_pauli(c));
    return contains(v);
}

PauliSetBDD PauliSetBDD::apply_X(int q) const {
    check_qubit(q, n_, "apply_X");
    // X is itself a Pauli-group element ((x,z)=(1,0)): composing it onto
    // every string means XOR-ing its x-bit, i.e. flip x_q across the set.
    return PauliSetBDD(substitute(f_, xvar(q), bdd_nithvar(xvar(q))));
}

PauliSetBDD PauliSetBDD::apply_Z(int q) const {
    check_qubit(q, n_, "apply_Z");
    // Z = (0,1): flip z_q across the set.
    return PauliSetBDD(substitute(f_, zvar(q), bdd_nithvar(zvar(q))));
}

PauliSetBDD PauliSetBDD::apply_H(int q) const {
    check_qubit(q, n_, "apply_H");
    // Conjugation: H·X·H=Z, H·Z·H=X -- a pure variable swap, no new
    // boolean structure needed.
    bddPair *pair = bdd_newpair();
    bdd_setpair(pair, xvar(q), zvar(q));
    bdd_setpair(pair, zvar(q), xvar(q));
    bdd result = bdd_replace(f_, pair);
    bdd_freepair(pair);
    return PauliSetBDD(result);
}

PauliSetBDD PauliSetBDD::apply_CX(int control, int target) const {
    check_qubit(control, n_, "apply_CX (control)");
    check_qubit(target, n_, "apply_CX (target)");
    if (control == target) {
        throw std::invalid_argument("apply_CX: control and target must differ");
    }
    // Conjugation: x_t' = x_t XOR x_c, z_c' = z_c XOR z_t, rest unchanged.
    // CX is an involution, so substituting the same forward map into f_'s
    // own variables computes the image set directly (see PROJECT_HANDOFF.md).
    // Both substitutions are done in one bdd_veccompose pass rather than two
    // sequential bdd_compose passes -- BuDDy's own docs note bdd_veccompose
    // is the efficient way to substitute several variables at once.
    bddPair *pair = bdd_newpair();
    bdd_setbddpair(pair, xvar(target), bdd_ithvar(xvar(target)) ^ bdd_ithvar(xvar(control)));
    bdd_setbddpair(pair, zvar(control), bdd_ithvar(zvar(control)) ^ bdd_ithvar(zvar(target)));
    bdd result = bdd_veccompose(f_, pair);
    bdd_freepair(pair);
    return PauliSetBDD(result);
}

PauliSetBDD PauliSetBDD::apply_CY(int control, int target) const {
    check_qubit(control, n_, "apply_CY (control)");
    check_qubit(target, n_, "apply_CY (target)");
    if (control == target) {
        throw std::invalid_argument("apply_CY: control and target must differ");
    }
    // Conjugation. Since S X S^ = Y, CY = (I@S) CX (I@S^), and conjugating the
    // generators through that gives
    //     X_c -> X_c Y_t,  Z_c -> Z_c,  X_t -> Z_c X_t,  Z_t -> Z_c Z_t
    // which in components is
    //     z_c' = z_c XOR x_t XOR z_t,  x_t' = x_t XOR x_c,  z_t' = z_t XOR x_c
    // with x_c untouched. CY is an involution (Y^2 = I), so substituting the
    // forward map into f_'s own variables gives the image set directly.
    //
    // Unlike CX and CZ, one substitution here reads variables that are
    // themselves being substituted: z_c' depends on x_t and z_t. That makes
    // bdd_veccompose's *simultaneous* semantics load-bearing rather than just
    // an optimisation -- two sequential bdd_compose calls would feed the
    // already-updated x_t/z_t into z_c' and get the wrong answer.
    bddPair *pair = bdd_newpair();
    bdd_setbddpair(pair, zvar(control),
                   bdd_ithvar(zvar(control)) ^ bdd_ithvar(xvar(target))
                       ^ bdd_ithvar(zvar(target)));
    bdd_setbddpair(pair, xvar(target), bdd_ithvar(xvar(target)) ^ bdd_ithvar(xvar(control)));
    bdd_setbddpair(pair, zvar(target), bdd_ithvar(zvar(target)) ^ bdd_ithvar(xvar(control)));
    bdd result = bdd_veccompose(f_, pair);
    bdd_freepair(pair);
    return PauliSetBDD(result);
}

PauliSetBDD PauliSetBDD::apply_CZ(int control, int target) const {
    check_qubit(control, n_, "apply_CZ (control)");
    check_qubit(target, n_, "apply_CZ (target)");
    if (control == target) {
        throw std::invalid_argument("apply_CZ: control and target must differ");
    }
    // Conjugation: z_c' = z_c XOR x_t, z_t' = z_t XOR x_c, rest unchanged.
    // CZ is also an involution; same reasoning and single-pass veccompose
    // as apply_CX.
    bddPair *pair = bdd_newpair();
    bdd_setbddpair(pair, zvar(control), bdd_ithvar(zvar(control)) ^ bdd_ithvar(xvar(target)));
    bdd_setbddpair(pair, zvar(target), bdd_ithvar(zvar(target)) ^ bdd_ithvar(xvar(control)));
    bdd result = bdd_veccompose(f_, pair);
    bdd_freepair(pair);
    return PauliSetBDD(result);
}
PauliSetBDD PauliSetBDD::multiply_by_all_paulis_on(const std::vector<int> &qubits) const {
    if (qubits.empty()) {
        return *this;  // multiplying by {identity} only
    }

    // Collect the 2 variables per qubit that the subgroup spans, rejecting
    // duplicates so bdd_makeset gets a genuine set (a repeated qubit would
    // otherwise silently shrink the varset it builds).
    std::vector<int> vars;
    vars.reserve(2 * qubits.size());
    std::vector<bool> seen(static_cast<size_t>(n_), false);
    for (int q : qubits) {
        check_qubit(q, n_, "multiply_by_all_paulis_on");
        if (seen[static_cast<size_t>(q)]) {
            throw std::invalid_argument("multiply_by_all_paulis_on: duplicate qubit index");
        }
        seen[static_cast<size_t>(q)] = true;
        vars.push_back(xvar(q));
        vars.push_back(zvar(q));
    }

    bdd varset = bdd_makeset(vars.data(), static_cast<int>(vars.size()));
    return PauliSetBDD(bdd_exist(f_, varset));
}

PauliSetBDD PauliSetBDD::reset_qubits(const std::vector<int> &qubits) const {
    if (qubits.empty()) return *this;

    // Forget first (this also validates the indices), then pin to identity so
    // the set does not keep the 4^|qubits| combinations the forget opened up.
    const PauliSetBDD forgotten = multiply_by_all_paulis_on(qubits);

    bdd identity = bddtrue;
    for (int q : qubits) {
        identity &= bdd_nithvar(xvar(q));
        identity &= bdd_nithvar(zvar(q));
    }
    return PauliSetBDD(forgotten.f_ & identity);
}

PauliSetBDD PauliSetBDD::multiply_by_all_paulis_on(int a, int b) const {
    if (a == b) {
        throw std::invalid_argument("multiply_by_all_paulis_on: the two qubits must differ");
    }
    return multiply_by_all_paulis_on(std::vector<int>{a, b});
}

namespace {

// Walk the diagram level by level, expanding every variable the current path
// skips into both of its values. A skipped variable is exactly a don't-care,
// so this turns the cubes BuDDy would hand back into concrete assignments.
void enumerate_rec(const bdd &node, int level, int nvars, int n_qubits,
                   std::vector<char> &assign, bool &keep_going,
                   const std::function<bool(const std::string &)> &fn) {
    if (!keep_going || node == bddfalse) return;

    if (level == nvars) {
        std::string s(static_cast<size_t>(n_qubits), 'I');
        for (int q = 0; q < n_qubits; ++q) {
            const int x = assign[static_cast<size_t>(PauliSetBDD::xvar(q))];
            const int z = assign[static_cast<size_t>(PauliSetBDD::zvar(q))];
            s[static_cast<size_t>(q)] =
                PauliSetBDD::pauli_to_char(static_cast<Pauli>((z << 1) | x));
        }
        keep_going = fn(s);
        return;
    }

    // bdd_var() is only meaningful on a non-terminal, hence the short circuit.
    const int  v        = bdd_level2var(level);
    const bool labelled = (node != bddtrue) && (bdd_var(node) == v);

    assign[static_cast<size_t>(v)] = 0;
    enumerate_rec(labelled ? bdd_low(node) : node, level + 1, nvars, n_qubits, assign,
                  keep_going, fn);
    assign[static_cast<size_t>(v)] = 1;
    enumerate_rec(labelled ? bdd_high(node) : node, level + 1, nvars, n_qubits, assign,
                  keep_going, fn);
}

} // namespace

void PauliSetBDD::for_each(const std::function<bool(const std::string &)> &fn) const {
    check_init();
    const int         nvars = 2 * n_;
    std::vector<char> assign(static_cast<size_t>(nvars), 0);
    bool              keep_going = true;
    enumerate_rec(f_, 0, nvars, n_, assign, keep_going, fn);
}

std::vector<std::string> PauliSetBDD::to_strings(std::size_t max_count) const {
    check_init();
    const double total = size();
    if (total > static_cast<double>(max_count)) {
        throw std::length_error("PauliSetBDD::to_strings: set holds "
                                + std::to_string(total) + " strings, above the cap of "
                                + std::to_string(max_count));
    }

    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(total));
    for_each([&out](const std::string &s) {
        out.push_back(s);
        return true;
    });
    return out;
}

bool   PauliSetBDD::is_empty() const   { return f_ == bddfalse; }
double PauliSetBDD::size() const       { return bdd_satcount(f_); }
int    PauliSetBDD::node_count() const { return bdd_nodecount(f_); }
void   PauliSetBDD::print_raw() const  { bdd_printset(f_); }

} // namespace pbdd
