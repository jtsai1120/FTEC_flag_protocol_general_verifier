#include "stabilizer.hpp"

#include <cassert>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace pbdd;

namespace {

// ---------------------------------------------------------------------------
// Brute-force reference: literally multiply every pair and test membership of
// N(S) \ S. Only usable on small codes and small sets, which is the point --
// it shares no code with the BDD path.
// ---------------------------------------------------------------------------

using Bits = std::vector<int>;   // 2n, index 2q = x_q, 2q+1 = z_q

Bits to_bits(const std::string &p) {
    Bits v(p.size() * 2, 0);
    for (size_t q = 0; q < p.size(); ++q) {
        const int c = static_cast<int>(PauliSetBDD::char_to_pauli(p[q]));
        v[2 * q]     = c & 1;
        v[2 * q + 1] = (c >> 1) & 1;
    }
    return v;
}

int symp(const Bits &u, const Bits &v) {
    int r = 0;
    for (size_t i = 0; i + 1 < u.size(); i += 2) r ^= (u[i] & v[i + 1]) ^ (u[i + 1] & v[i]);
    return r & 1;
}

Bits xor_bits(const Bits &a, const Bits &b) {
    Bits o(a.size());
    for (size_t i = 0; i < a.size(); ++i) o[i] = a[i] ^ b[i];
    return o;
}

// Is `v` in the span of `gens`? Gaussian elimination each time; fine at this size.
bool in_span(const std::vector<Bits> &gens, Bits v) {
    std::vector<Bits> basis;
    for (const Bits &g : gens) {
        Bits cur = g;
        for (const Bits &b : basis) {
            size_t lead = 0;
            while (lead < b.size() && !b[lead]) ++lead;
            if (lead < cur.size() && cur[lead]) cur = xor_bits(cur, b);
        }
        bool nonzero = false;
        for (int x : cur) nonzero |= x != 0;
        if (nonzero) basis.push_back(cur);
    }
    for (const Bits &b : basis) {
        size_t lead = 0;
        while (lead < b.size() && !b[lead]) ++lead;
        if (lead < v.size() && v[lead]) v = xor_bits(v, b);
    }
    for (int x : v) {
        if (x) return false;
    }
    return true;
}

bool brute_force_has_collision(const std::vector<std::string> &gens,
                               const std::vector<std::string> &elements) {
    std::vector<Bits> gb;
    for (const std::string &g : gens) gb.push_back(to_bits(g));

    for (size_t i = 0; i < elements.size(); ++i) {
        for (size_t j = i + 1; j < elements.size(); ++j) {
            const Bits prod = xor_bits(to_bits(elements[i]), to_bits(elements[j]));

            bool in_normaliser = true;                       // commutes with every generator
            for (const Bits &g : gb) in_normaliser &= symp(g, prod) == 0;
            if (!in_normaliser) continue;

            if (!in_span(gb, prod)) return true;              // in N(S), not in S
        }
    }
    return false;
}

// ---------------------------------------------------------------------------

PauliSetBDD set_of(const std::vector<std::string> &elements) {
    PauliSetBDD s = PauliSetBDD::empty();
    for (const std::string &e : elements) s = s | PauliSetBDD::single(e);
    return s;
}

// Pad a data-qubit Pauli out to the full register width.
std::string pad(const std::string &p, int total) {
    return p + std::string(static_cast<size_t>(total) - p.size(), 'I');
}

const std::vector<std::string> kSteane = {"IIIXXXX", "IXXIIXX", "XIXIXIX",
                                          "IIIZZZZ", "IZZIIZZ", "ZIZIZIZ"};

// [[5,1,3]] perfect code.
const std::vector<std::string> kFive = {"XZZXI", "IXZZX", "XIXZZ", "ZXIXZ"};

void check_against_brute_force(const std::vector<std::string> &gens, int n,
                               const std::vector<std::string> &elements, const char *label) {
    const StabilizerCode code(n, gens);
    const bool expected = brute_force_has_collision(gens, elements);
    const LogicalCollision got = find_undetectable_logical_pair(code, set_of(elements));

    assert(got.found == expected);
    if (got.found) {
        // The witnesses really do multiply into N(S) \ S, and they really are
        // in the set we handed over.
        assert(brute_force_has_collision(gens, {got.witness_1, got.witness_2}));
        assert(set_of(elements).contains(got.witness_1));
        assert(set_of(elements).contains(got.witness_2));
        assert(code.syndrome_of(got.witness_1) == got.syndrome);
        assert(code.syndrome_of(got.witness_2) == got.syndrome);
        assert(code.logical_of(got.witness_1) == got.logical_a);
        assert(code.logical_of(got.witness_2) == got.logical_b);
        assert(got.logical_a != got.logical_b);
    }
    std::cout << "    " << label << ": " << (expected ? "collision" : "clean")
              << ", matches brute force\n";
}

} // namespace

int main() {
    std::cout << "== construction / validation ==\n";
    {
        PauliSetBDD::init(7);
        const StabilizerCode steane(7, kSteane);
        assert(steane.n_data() == 7 && steane.k() == 6 && steane.m() == 1);
        std::cout << "    Steane [[7,1,3]]: k = 6, m = 1\n";

        // Generators are in S, so they have zero syndrome and zero logical part.
        for (const std::string &g : kSteane) {
            assert(steane.syndrome_of(g) == std::string(6, '0'));
            assert(steane.logical_of(g) == "00");
        }
        // A weight-7 logical X commutes with everything but is not a stabilizer.
        assert(steane.syndrome_of("XXXXXXX") == std::string(6, '0'));
        assert(steane.logical_of("XXXXXXX") != "00");
        assert(steane.syndrome_of("ZZZZZZZ") == std::string(6, '0'));
        assert(steane.logical_of("ZZZZZZZ") != "00");
        // A single-qubit error is detected: non-zero syndrome.
        assert(steane.syndrome_of("XIIIIII") != std::string(6, '0'));
        std::cout << "    syndrome_of / logical_of agree with the code's structure\n";

        bool threw = false;
        try { StabilizerCode(7, {"XIIIIII", "ZIIIIII"}); }
        catch (const std::invalid_argument &) { threw = true; }
        assert(threw);                                     // do not commute

        threw = false;
        try { StabilizerCode(4, {"XXII", "IIXX", "XXXX"}); }
        catch (const std::invalid_argument &) { threw = true; }
        assert(threw);                                     // dependent

        threw = false;
        try { StabilizerCode(2, {"XX", "ZZ"}); }
        catch (const std::invalid_argument &) { threw = true; }
        assert(threw);                                     // m = 0

        threw = false;
        try { StabilizerCode(7, {"XXX"}); }
        catch (const std::invalid_argument &) { threw = true; }
        assert(threw);                                     // wrong length
        std::cout << "    rejects non-commuting / dependent / m=0 / malformed input\n";
        PauliSetBDD::done();
    }

    std::cout << "\n== Steane code, hand-checked cases ==\n";
    {
        PauliSetBDD::init(7);
        const StabilizerCode code(7, kSteane);

        // Distance 3: no two weight-<=1 errors can multiply into N(S)\S.
        std::vector<std::string> weight1{"IIIIIII"};
        for (int q = 0; q < 7; ++q) {
            for (char p : std::string("XZY")) {
                std::string s(7, 'I');
                s[static_cast<size_t>(q)] = p;
                weight1.push_back(s);
            }
        }
        assert(!has_undetectable_logical_pair(code, set_of(weight1)));
        std::cout << "    all weight-1 errors: clean (distance 3)\n";

        // Identity together with a logical operator is the minimal failure.
        const LogicalCollision hit =
            find_undetectable_logical_pair(code, set_of({"IIIIIII", "XXXXXXX"}));
        assert(hit.found);
        assert(hit.syndrome == std::string(6, '0'));
        assert(hit.logical_a != hit.logical_b);
        std::cout << "    {I, logical X}: collision, syndrome " << hit.syndrome
                  << ", logicals " << hit.logical_a << " vs " << hit.logical_b << "\n";
        std::cout << "      witnesses: " << hit.witness_1 << " and " << hit.witness_2 << "\n";

        // A stabilizer is not a logical error: {I, g} must stay clean.
        assert(!has_undetectable_logical_pair(code, set_of({"IIIIIII", kSteane[0]})));
        std::cout << "    {I, stabilizer}: clean (product lies in S)\n";

        // Same syndrome, different logical class -> collision. XIIIIII and a
        // weight-2 partner that differs from it by a logical operator.
        const std::string e1 = "XIIIIII";
        std::string       e2(7, 'I');
        for (int q = 0; q < 7; ++q) {
            if (q != 0) e2[static_cast<size_t>(q)] = 'X';   // e1 * e2 = XXXXXXX
        }
        assert(code.syndrome_of(e1) == code.syndrome_of(e2));
        assert(code.logical_of(e1) != code.logical_of(e2));
        assert(has_undetectable_logical_pair(code, set_of({e1, e2})));
        std::cout << "    same syndrome, different logical class: collision\n";

        // Different syndromes can never collide, however many elements.
        std::vector<std::string> distinct_syndromes;
        std::set<std::string>    seen;
        for (int q = 0; q < 7; ++q) {
            std::string s(7, 'I');
            s[static_cast<size_t>(q)] = 'X';
            if (seen.insert(code.syndrome_of(s)).second) distinct_syndromes.push_back(s);
        }
        assert(!has_undetectable_logical_pair(code, set_of(distinct_syndromes)));
        std::cout << "    pairwise distinct syndromes: clean, as the algebra requires\n";
        PauliSetBDD::done();
    }

    std::cout << "\n== agreement with brute force ==\n";
    {
        PauliSetBDD::init(5);
        check_against_brute_force(kFive, 5, {"IIIII", "XIIII", "IXIII"}, "[[5,1,3]] weight-1");
        check_against_brute_force(kFive, 5, {"IIIII", "XXXXX"}, "[[5,1,3]] with a logical");
        check_against_brute_force(kFive, 5, {"XZZXI", "IIIII"}, "[[5,1,3]] with a stabilizer");

        std::mt19937 rng(7);
        std::uniform_int_distribution<int> pick(0, 3);
        int collisions = 0;
        for (int trial = 0; trial < 60; ++trial) {
            std::vector<std::string> elements;
            const int count = 2 + trial % 5;
            for (int e = 0; e < count; ++e) {
                std::string s;
                for (int q = 0; q < 5; ++q) {
                    s += PauliSetBDD::pauli_to_char(static_cast<Pauli>(pick(rng)));
                }
                elements.push_back(s);
            }
            const StabilizerCode code(5, kFive);
            const bool expected = brute_force_has_collision(kFive, elements);
            const LogicalCollision got = find_undetectable_logical_pair(code, set_of(elements));
            assert(got.found == expected);
            if (got.found) {
                ++collisions;
                assert(brute_force_has_collision(kFive, {got.witness_1, got.witness_2}));
                assert(set_of(elements).contains(got.witness_1));
                assert(set_of(elements).contains(got.witness_2));
            }
        }
        std::cout << "    60 random [[5,1,3]] sets match brute force (" << collisions
                  << " collisions)\n";
        PauliSetBDD::done();
    }
    {
        PauliSetBDD::init(7);
        std::mt19937 rng(11);
        std::uniform_int_distribution<int> pick(0, 3);
        int collisions = 0;
        for (int trial = 0; trial < 40; ++trial) {
            std::vector<std::string> elements;
            for (int e = 0; e < 4; ++e) {
                std::string s;
                for (int q = 0; q < 7; ++q) {
                    s += PauliSetBDD::pauli_to_char(static_cast<Pauli>(pick(rng)));
                }
                elements.push_back(s);
            }
            const StabilizerCode code(7, kSteane);
            const bool expected = brute_force_has_collision(kSteane, elements);
            assert(has_undetectable_logical_pair(code, set_of(elements)) == expected);
            collisions += expected;
        }
        std::cout << "    40 random Steane sets match brute force (" << collisions
                  << " collisions)\n";
        PauliSetBDD::done();
    }

    std::cout << "\n== larger sets and extra registers ==\n";
    {
        // The code covers only the first 5 qubits; the rest is a measurement
        // register that must be projected away.
        PauliSetBDD::init(7);
        const StabilizerCode code(5, kFive);

        assert(!has_undetectable_logical_pair(
            code, set_of({pad("IIIII", 7), pad("XIIII", 7)})));
        assert(has_undetectable_logical_pair(
            code, set_of({pad("IIIII", 7), pad("XXXXX", 7)})));

        // Ancilla content must not change the verdict.
        assert(has_undetectable_logical_pair(code, set_of({"IIIIIXY", "XXXXXZI"})));
        std::cout << "    the measurement register is projected away correctly\n";

        // The whole universe certainly contains a colliding pair.
        assert(has_undetectable_logical_pair(code, PauliSetBDD::universe()));
        // A single element cannot collide with itself.
        assert(!has_undetectable_logical_pair(code, PauliSetBDD::single("XXXXXII")));
        assert(!has_undetectable_logical_pair(code, PauliSetBDD::empty()));
        std::cout << "    universe collides; singletons and the empty set do not\n";
        PauliSetBDD::done();
    }

    std::cout << "\n== the verdict does not depend on the chosen logicals ==\n";
    {
        // The logical representatives are *generated*, not supplied, and they
        // are not unique. Different generating sets for the same group send
        // Gram-Schmidt down different paths, so the destabilizers and the
        // syndrome bit *order* change -- but "product lies in N(S)\S" is a
        // statement about the group, so every verdict must survive that.
        PauliSetBDD::init(7);
        const std::vector<std::vector<std::string>> equivalent = {
            kSteane,
            {"ZIZIZIZ", "IZZIIZZ", "IIIZZZZ", "XIXIXIX", "IXXIIXX", "IIIXXXX"},
            {"IXXXXII", "IXXIIXX", "XIXIXIX", "IZZZZII", "IZZIIZZ", "ZIZIZIZ"},
            {"XIXIXIX", "IIIZZZZ", "IIIXXXX", "ZIZIZIZ", "IXXIIXX", "IZZIIZZ"},
        };

        // A handful of sets spanning both answers.
        const std::vector<std::vector<std::string>> probes = {
            {"IIIIIII", "XXXXXXX"},                 // collides: a logical
            {"IIIIIII", kSteane[0]},                // clean: a stabilizer
            {"ZIIIIII", "IIIIIZZ"},                 // collides: weight-3 logical Z
            {"XIIIIII", "IXIIIII", "IIXIIII"},      // clean: distance 3
        };

        for (const auto &probe : probes) {
            const PauliSetBDD s = set_of(probe);
            const bool        expected = brute_force_has_collision(kSteane, probe);
            for (const auto &gens : equivalent) {
                const StabilizerCode code(7, gens);
                const LogicalCollision hit = find_undetectable_logical_pair(code, s);
                assert(hit.found == expected);
                if (hit.found) {
                    // Whatever labels this basis produced, the witnesses really
                    // do multiply into N(S)\S.
                    assert(hit.logical_a != hit.logical_b);
                    assert(brute_force_has_collision(kSteane, {hit.witness_1, hit.witness_2}));
                }
            }
        }
        std::cout << "    4 generating sets x 4 probe sets: identical verdicts\n";

        // The labels themselves are allowed to move, and do: reordering the
        // generators permutes the syndrome bits.
        const StabilizerCode a(7, equivalent[0]);
        const StabilizerCode b(7, equivalent[1]);
        assert(a.syndrome_of("ZIIIIII") != b.syndrome_of("ZIIIIII"));
        // But "is this in S" never moves.
        for (const std::string &p : {std::string("XXXXXXX"), std::string("ZIIIIZZ")}) {
            assert((a.logical_of(p) == "00") == (b.logical_of(p) == "00"));
            assert(a.logical_of(p) != "00");   // both are non-trivial logicals
        }
        std::cout << "    syndrome labels move with the basis; membership of S does not\n";
        PauliSetBDD::done();
    }

    std::cout << "\nAll stabilizer tests passed.\n";
    return 0;
}
