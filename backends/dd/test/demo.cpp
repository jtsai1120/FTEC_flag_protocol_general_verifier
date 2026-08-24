#include "pauli_bdd.hpp"

#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace pbdd;

int main() {
    const int n = 3;
    PauliSetBDD::init(n);

    std::cout << "num_qubits = " << PauliSetBDD::num_qubits() << "\n";

    // Basic singleton construction + membership
    PauliSetBDD p1 = PauliSetBDD::single("XII");
    PauliSetBDD p2 = PauliSetBDD::single("IYI");
    PauliSetBDD p3 = PauliSetBDD::single("ZZZ");

    assert(p1.contains("XII"));
    assert(!p1.contains("IYI"));
    assert(p1.size() == 1.0);
    std::cout << "single-operator membership tests passed\n";

    // Union
    PauliSetBDD s = p1 | p2 | p3;
    assert(s.contains("XII"));
    assert(s.contains("IYI"));
    assert(s.contains("ZZZ"));
    assert(!s.contains("III"));
    assert(s.size() == 3.0);
    std::cout << "union set size = " << s.size() << " (expected 3)\n";

    // Intersection
    PauliSetBDD t = (p1 | p2) & (p2 | p3);
    assert(t == p2);
    assert(t.size() == 1.0);
    std::cout << "intersection test passed\n";

    // Set difference
    PauliSetBDD d = s - p2;
    assert(!d.contains("IYI"));
    assert(d.contains("XII"));
    assert(d.size() == 2.0);
    std::cout << "difference test passed\n";

    // Complement + universe size check: 4^3 = 64
    PauliSetBDD full = PauliSetBDD::universe();
    assert(full.size() == 64.0);
    PauliSetBDD comp = !s;
    assert((comp & s).is_empty());
    assert(comp.size() + s.size() == 64.0);
    std::cout << "universe size = " << full.size() << " (expected 64), "
              << "complement size = " << comp.size() << " (expected 61)\n";

    // Empty set
    PauliSetBDD e = PauliSetBDD::empty();
    assert(e.is_empty());
    assert(e.size() == 0.0);

    // Equality / inequality
    PauliSetBDD s2 = p3 | p2 | p1; // same set, built in a different order
    assert(s == s2);
    assert(s != p1);
    std::cout << "equality test passed (order of construction does not matter)\n";

    // Enumeration: BuDDy's own bdd_allsat yields cubes with don't-cares, so
    // to_strings() expands them into concrete Pauli strings.
    {
        assert(PauliSetBDD::empty().to_strings().empty());
        assert(p1.to_strings() == std::vector<std::string>{"XII"});

        const std::vector<std::string> listed = s.to_strings();
        assert(listed.size() == 3);
        const std::set<std::string> as_set(listed.begin(), listed.end());
        assert(as_set == (std::set<std::string>{"XII", "IYI", "ZZZ"}));
        for (const auto &m : listed) assert(s.contains(m));

        // A set full of don't-cares still enumerates one string at a time:
        // bdd_allsat would report the 64-element universe as a single cube.
        const std::vector<std::string> everything = full.to_strings();
        assert(everything.size() == 64);
        assert(std::set<std::string>(everything.begin(), everything.end()).size() == 64);

        // for_each can stop early.
        int seen = 0;
        full.for_each([&seen](const std::string &) { return ++seen < 5; });
        assert(seen == 5);

        // The cap guards against accidentally enumerating 4^n strings.
        bool threw = false;
        try { full.to_strings(10); } catch (const std::length_error &) { threw = true; }
        assert(threw);

        std::cout << "enumeration tests passed (universe lists " << everything.size()
                  << " distinct strings)\n";
    }

    // Diagnostics
    std::cout << "node_count(s) = " << s.node_count() << "\n";

    std::cout << "\nAll tests passed.\n";

    PauliSetBDD::done();
    return 0;
}
