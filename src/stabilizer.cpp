#include "stabilizer.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace pbdd {

namespace {

using Vec = std::vector<unsigned char>;
using Mat = std::vector<Vec>;

// --- GF(2) helpers ---------------------------------------------------------

void xor_into(Vec &dst, const Vec &src) {
    for (size_t i = 0; i < dst.size(); ++i) dst[i] ^= src[i];
}

// Symplectic product, with the interleaved layout: index 2q is x_q, 2q+1 is z_q.
// <u,v> = sum_q (u.x_q & v.z_q) ^ (u.z_q & v.x_q). Zero means the two Paulis
// commute.
unsigned char symp(const Vec &u, const Vec &v) {
    unsigned char r = 0;
    for (size_t i = 0; i + 1 < u.size(); i += 2) {
        r ^= static_cast<unsigned char>((u[i] & v[i + 1]) ^ (u[i + 1] & v[i]));
    }
    return r & 1u;
}

// <v, .> as a plain dot product: swapping x and z within each qubit turns the
// symplectic form into the standard one.
Vec swap_xz(const Vec &v) {
    Vec out(v.size());
    for (size_t i = 0; i + 1 < v.size(); i += 2) {
        out[i]     = v[i + 1];
        out[i + 1] = v[i];
    }
    return out;
}

Mat identity(int n) {
    Mat I(static_cast<size_t>(n), Vec(static_cast<size_t>(n), 0));
    for (int i = 0; i < n; ++i) I[static_cast<size_t>(i)][static_cast<size_t>(i)] = 1;
    return I;
}

// Reduce `rows` to reduced row echelon form over the first `ncols` columns,
// applying the same operations to `aug` when given. Returns the pivot column of
// each surviving row, so its size is the rank.
std::vector<int> rref(Mat &rows, Mat *aug, int ncols) {
    std::vector<int> pivots;
    size_t           r = 0;

    for (int c = 0; c < ncols && r < rows.size(); ++c) {
        size_t p = rows.size();
        for (size_t i = r; i < rows.size(); ++i) {
            if (rows[i][static_cast<size_t>(c)]) { p = i; break; }
        }
        if (p == rows.size()) continue;

        std::swap(rows[r], rows[p]);
        if (aug) std::swap((*aug)[r], (*aug)[p]);

        for (size_t i = 0; i < rows.size(); ++i) {
            if (i != r && rows[i][static_cast<size_t>(c)]) {
                xor_into(rows[i], rows[r]);
                if (aug) xor_into((*aug)[i], (*aug)[r]);
            }
        }
        pivots.push_back(c);
        ++r;
    }
    return pivots;
}

// Basis of { v : M v = 0 }, vectors of length ncols.
Mat nullspace(Mat m, int ncols) {
    const std::vector<int> piv = rref(m, nullptr, ncols);

    std::vector<bool> is_pivot(static_cast<size_t>(ncols), false);
    for (int c : piv) is_pivot[static_cast<size_t>(c)] = true;

    Mat basis;
    for (int c = 0; c < ncols; ++c) {
        if (is_pivot[static_cast<size_t>(c)]) continue;
        Vec v(static_cast<size_t>(ncols), 0);
        v[static_cast<size_t>(c)] = 1;
        for (size_t j = 0; j < piv.size(); ++j) {
            v[static_cast<size_t>(piv[j])] = m[j][static_cast<size_t>(c)];
        }
        basis.push_back(v);
    }
    return basis;
}

Mat invert(Mat m) {
    const int n = static_cast<int>(m.size());
    Mat       inv = identity(n);
    const std::vector<int> piv = rref(m, &inv, n);
    if (static_cast<int>(piv.size()) != n) {
        throw std::logic_error("StabilizerCode: symplectic basis matrix is singular");
    }
    return inv;
}

// --- BDD helpers -----------------------------------------------------------

bdd cube_of(const std::vector<int> &vars, const std::vector<unsigned char> &bits) {
    bdd c = bddtrue;
    for (size_t i = 0; i < vars.size(); ++i) {
        c &= bits[i] ? bdd_ithvar(vars[i]) : bdd_nithvar(vars[i]);
    }
    return c;
}

bdd all_zero(const std::vector<int> &vars) {
    bdd c = bddtrue;
    for (int v : vars) c &= bdd_nithvar(v);
    return c;
}

bdd varset_of(std::vector<int> vars) {
    if (vars.empty()) return bddtrue;
    return bdd_makeset(vars.data(), static_cast<int>(vars.size()));
}

// One element of the set, as raw variable bits. Reuses the tested enumeration
// and stops after the first hit. Returns an empty vector for an empty set.
std::vector<unsigned char> one_assignment(const PauliSetBDD &s) {
    std::vector<unsigned char> bits;
    s.for_each([&bits](const std::string &str) {
        bits.assign(str.size() * 2, 0);
        for (size_t q = 0; q < str.size(); ++q) {
            const unsigned char p =
                static_cast<unsigned char>(PauliSetBDD::char_to_pauli(str[q]));
            bits[2 * q]     = p & 1u;
            bits[2 * q + 1] = (p >> 1) & 1u;
        }
        return false;
    });
    return bits;
}

std::string bits_to_string(const std::vector<unsigned char> &bits,
                           const std::vector<int> &vars) {
    std::string out(vars.size(), '0');
    for (size_t i = 0; i < vars.size(); ++i) {
        out[i] = bits[static_cast<size_t>(vars[i])] ? '1' : '0';
    }
    return out;
}

std::string bits_to_pauli(const std::vector<unsigned char> &bits, int n) {
    std::string s(static_cast<size_t>(n), 'I');
    for (int q = 0; q < n; ++q) {
        const unsigned char v = static_cast<unsigned char>(
            (bits[static_cast<size_t>(2 * q + 1)] << 1) | bits[static_cast<size_t>(2 * q)]);
        s[static_cast<size_t>(q)] = PauliSetBDD::pauli_to_char(static_cast<Pauli>(v));
    }
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// StabilizerCode
// ---------------------------------------------------------------------------

StabilizerCode::Vec StabilizerCode::parse(const std::string &pauli, const char *who) const {
    if (static_cast<int>(pauli.size()) != n_) {
        throw std::invalid_argument(std::string(who) + ": expected a Pauli string of length "
                                    + std::to_string(n_) + ", got \"" + pauli + "\"");
    }
    Vec v(static_cast<size_t>(2 * n_), 0);
    for (int q = 0; q < n_; ++q) {
        const unsigned char p = static_cast<unsigned char>(
            PauliSetBDD::char_to_pauli(pauli[static_cast<size_t>(q)]));
        v[static_cast<size_t>(2 * q)]     = p & 1u;
        v[static_cast<size_t>(2 * q + 1)] = (p >> 1) & 1u;
    }
    return v;
}

StabilizerCode::StabilizerCode(int n_data, const std::vector<std::string> &generators) {
    if (n_data <= 0) throw std::invalid_argument("StabilizerCode: n_data must be positive");
    if (generators.empty()) {
        throw std::invalid_argument("StabilizerCode: at least one generator is required");
    }

    n_ = n_data;
    k_ = static_cast<int>(generators.size());
    m_ = n_ - k_;
    if (m_ <= 0) {
        throw std::invalid_argument(
            "StabilizerCode: the generators leave no logical qubits (m = " + std::to_string(m_)
            + "); with N(S) = S no pair of errors can multiply into N(S)\\S");
    }

    const int width = 2 * n_;

    for (const std::string &s : generators) g_.push_back(parse(s, "StabilizerCode"));

    // Generators must pairwise commute, or they do not generate a stabilizer group.
    for (int i = 0; i < k_; ++i) {
        for (int j = i + 1; j < k_; ++j) {
            if (symp(g_[static_cast<size_t>(i)], g_[static_cast<size_t>(j)])) {
                throw std::invalid_argument("StabilizerCode: generators " + std::to_string(i)
                                            + " and " + std::to_string(j)
                                            + " do not commute");
            }
        }
    }

    // Destabilizers. <g_i, .> is the plain dot product with swap_xz(g_i), so
    // solving for <g_i, d_j> = delta_ij is one linear system. Row-reduce the
    // generator matrix while recording the row operations, then read off a
    // particular solution at the pivot columns.
    Mat a;
    for (const Vec &g : g_) a.push_back(swap_xz(g));
    Mat       c   = identity(k_);
    const std::vector<int> piv = rref(a, &c, width);
    if (static_cast<int>(piv.size()) != k_) {
        throw std::invalid_argument("StabilizerCode: generators are not linearly independent");
    }

    for (int i = 0; i < k_; ++i) {
        Vec u(static_cast<size_t>(width), 0);
        for (int j = 0; j < k_; ++j) {
            u[static_cast<size_t>(piv[static_cast<size_t>(j)])] =
                c[static_cast<size_t>(j)][static_cast<size_t>(i)];
        }
        d_.push_back(u);
    }
    // Make the destabilizers commute with each other. Adding g_i to d_j flips
    // <d_i,d_j> and nothing else, since <g_i,g_l> = 0 and <g_l,d_j> = delta_lj.
    for (int j = 0; j < k_; ++j) {
        for (int i = 0; i < j; ++i) {
            if (symp(d_[static_cast<size_t>(i)], d_[static_cast<size_t>(j)])) {
                xor_into(d_[static_cast<size_t>(j)], g_[static_cast<size_t>(i)]);
            }
        }
    }

    // Logicals: whatever is symplectically orthogonal to every g and d. That
    // space has dimension 2n - 2k = 2m; split it into hyperbolic pairs.
    Mat constraints;
    for (const Vec &g : g_) constraints.push_back(swap_xz(g));
    for (const Vec &d : d_) constraints.push_back(swap_xz(d));
    Mat rest = nullspace(constraints, width);
    if (static_cast<int>(rest.size()) != 2 * m_) {
        throw std::logic_error("StabilizerCode: logical subspace has unexpected dimension");
    }

    for (int j = 0; j < m_; ++j) {
        const Vec v = rest.front();
        size_t    partner = rest.size();
        for (size_t t = 1; t < rest.size(); ++t) {
            if (symp(v, rest[t])) { partner = t; break; }
        }
        if (partner == rest.size()) {
            throw std::logic_error("StabilizerCode: logical subspace is degenerate");
        }
        const Vec w = rest[partner];
        x_.push_back(v);
        z_.push_back(w);

        Mat next;
        for (size_t t = 1; t < rest.size(); ++t) {
            if (t == partner) continue;
            Vec u = rest[t];
            if (symp(u, w)) xor_into(u, v);
            if (symp(u, v)) xor_into(u, w);
            next.push_back(u);
        }
        rest = next;
    }

    // Coordinate map T: row r says how new coordinate r reads off an error.
    //   rows 0..k-1        a_i = <e,d_i>
    //   rows k..2k-1       b_i = <e,g_i>    (the syndrome)
    //   rows 2k+2j, +1     c_j = <e,Z_j>, f_j = <e,X_j>
    Mat t(static_cast<size_t>(width));
    for (int i = 0; i < k_; ++i) {
        t[static_cast<size_t>(i)]          = swap_xz(d_[static_cast<size_t>(i)]);
        t[static_cast<size_t>(k_ + i)]     = swap_xz(g_[static_cast<size_t>(i)]);
    }
    for (int j = 0; j < m_; ++j) {
        t[static_cast<size_t>(2 * k_ + 2 * j)]     = swap_xz(z_[static_cast<size_t>(j)]);
        t[static_cast<size_t>(2 * k_ + 2 * j + 1)] = swap_xz(x_[static_cast<size_t>(j)]);
    }

    // The BDD substitution needs the inverse: the image set's characteristic
    // function is chi_B(T^-1 y), so variable e_i becomes the XOR of the y's
    // selected by row i of T^-1.
    tinv_ = invert(t);
}

std::string StabilizerCode::logical_x(int j) const {
    if (j < 0 || j >= m_) throw std::out_of_range("StabilizerCode::logical_x");
    return bits_to_pauli(x_[static_cast<size_t>(j)], n_);
}

std::string StabilizerCode::logical_z(int j) const {
    if (j < 0 || j >= m_) throw std::out_of_range("StabilizerCode::logical_z");
    return bits_to_pauli(z_[static_cast<size_t>(j)], n_);
}

std::string StabilizerCode::destabilizer(int i) const {
    if (i < 0 || i >= k_) throw std::out_of_range("StabilizerCode::destabilizer");
    return bits_to_pauli(d_[static_cast<size_t>(i)], n_);
}

std::string StabilizerCode::syndrome_of(const std::string &pauli) const {
    const Vec   v = parse(pauli, "syndrome_of");
    std::string s(static_cast<size_t>(k_), '0');
    for (int i = 0; i < k_; ++i) {
        s[static_cast<size_t>(i)] = symp(g_[static_cast<size_t>(i)], v) ? '1' : '0';
    }
    return s;
}

std::string StabilizerCode::logical_of(const std::string &pauli) const {
    const Vec   v = parse(pauli, "logical_of");
    std::string s(static_cast<size_t>(2 * m_), '0');
    for (int j = 0; j < m_; ++j) {
        s[static_cast<size_t>(2 * j)]     = symp(v, z_[static_cast<size_t>(j)]) ? '1' : '0';
        s[static_cast<size_t>(2 * j + 1)] = symp(v, x_[static_cast<size_t>(j)]) ? '1' : '0';
    }
    return s;
}

// ---------------------------------------------------------------------------
// The query
// ---------------------------------------------------------------------------

LogicalCollision find_undetectable_logical_pair(const StabilizerCode &code,
                                                const PauliSetBDD &set) {
    const int total = PauliSetBDD::num_qubits();
    if (total < code.n_data()) {
        throw std::invalid_argument("find_undetectable_logical_pair: the package has "
                                    + std::to_string(total) + " qubits, fewer than the code's "
                                    + std::to_string(code.n_data()));
    }

    const int k = code.k();
    const int m = code.m();

    // Variable slots in the transformed coordinates.
    std::vector<int> a_vars, b_vars, l_vars, extra_vars;
    for (int i = 0; i < k; ++i) a_vars.push_back(i);
    for (int i = 0; i < k; ++i) b_vars.push_back(k + i);
    for (int j = 0; j < 2 * m; ++j) l_vars.push_back(2 * k + j);
    for (int q = code.n_data(); q < total; ++q) {          // measurement register
        extra_vars.push_back(PauliSetBDD::xvar(q));
        extra_vars.push_back(PauliSetBDD::zvar(q));
    }

    LogicalCollision result;

    // The stabilizers only touch the data qubits, so forget everything else.
    PauliSetBDD data = set;
    if (!extra_vars.empty()) {
        std::vector<int> qs;
        for (int q = code.n_data(); q < total; ++q) qs.push_back(q);
        data = data.multiply_by_all_paulis_on(qs);
    }
    if (data.is_empty()) return result;

    // Move into (a, b, c, f) coordinates. Simultaneous substitution is
    // mandatory here, not an optimisation: every new coordinate is an XOR of
    // the very variables being replaced.
    bddPair *pair = bdd_newpair();
    for (int v = 0; v < 2 * code.n_data(); ++v) {
        bdd expr = bddfalse;
        for (int r = 0; r < 2 * code.n_data(); ++r) {
            if (code.tinv_[static_cast<size_t>(v)][static_cast<size_t>(r)]) {
                expr ^= bdd_ithvar(r);
            }
        }
        bdd_setbddpair(pair, v, expr);
    }
    const PauliSetBDD transformed(bdd_veccompose(data.raw(), pair));
    bdd_freepair(pair);

    // Forgetting the destabilizer component is exactly "multiply by S": the
    // fibres of (syndrome, logical) are the cosets of S, and in these
    // coordinates S is axis-aligned, so it is one quantification.
    const bdd fibres = bdd_exist(transformed.raw(), varset_of(a_vars));

    // A fibre holds two or more logical signatures iff some logical variable
    // takes both values inside it -- two distinct points must differ somewhere.
    const bdd l_set = varset_of(l_vars);
    bdd       multi = bddfalse;
    for (int v : l_vars) {
        const bdd with0 = bdd_exist(fibres & bdd_nithvar(v), l_set);
        const bdd with1 = bdd_exist(fibres & bdd_ithvar(v), l_set);
        multi |= with0 & with1;
    }
    if (multi == bddfalse) return result;

    result.found = true;

    // Pin the free coordinates so a single satisfying assignment reads cleanly.
    std::vector<int> free_vars = a_vars;
    free_vars.insert(free_vars.end(), extra_vars.begin(), extra_vars.end());

    const std::vector<unsigned char> b_bits =
        one_assignment(PauliSetBDD(multi & all_zero(free_vars) & all_zero(l_vars)));
    result.syndrome = bits_to_string(b_bits, b_vars);

    std::vector<unsigned char> chosen_b;
    for (int v : b_vars) chosen_b.push_back(b_bits[static_cast<size_t>(v)]);
    const bdd b_cube = cube_of(b_vars, chosen_b);

    // Two different logical signatures inside that fibre.
    const bdd fibre = fibres & b_cube;
    const std::vector<unsigned char> la_bits =
        one_assignment(PauliSetBDD(fibre & all_zero(free_vars)));
    result.logical_a = bits_to_string(la_bits, l_vars);

    std::vector<unsigned char> chosen_la;
    for (int v : l_vars) chosen_la.push_back(la_bits[static_cast<size_t>(v)]);
    const bdd la_cube = cube_of(l_vars, chosen_la);

    const std::vector<unsigned char> lb_bits =
        one_assignment(PauliSetBDD(fibre & !la_cube & all_zero(free_vars)));
    result.logical_b = bits_to_string(lb_bits, l_vars);

    std::vector<unsigned char> chosen_lb;
    for (int v : l_vars) chosen_lb.push_back(lb_bits[static_cast<size_t>(v)]);
    const bdd lb_cube = cube_of(l_vars, chosen_lb);

    // Concrete witnesses: pick a full point of each class and map it back.
    auto witness = [&](const bdd &l_cube) {
        const std::vector<unsigned char> y =
            one_assignment(PauliSetBDD(transformed.raw() & b_cube & l_cube));
        std::vector<unsigned char> e(static_cast<size_t>(2 * code.n_data()), 0);
        for (int v = 0; v < 2 * code.n_data(); ++v) {
            unsigned char bit = 0;
            for (int r = 0; r < 2 * code.n_data(); ++r) {
                bit ^= static_cast<unsigned char>(
                    code.tinv_[static_cast<size_t>(v)][static_cast<size_t>(r)]
                    & y[static_cast<size_t>(r)]);
            }
            e[static_cast<size_t>(v)] = bit & 1u;
        }
        return bits_to_pauli(e, code.n_data());
    };

    result.witness_1 = witness(la_cube);
    result.witness_2 = witness(lb_cube);
    return result;
}

} // namespace pbdd
