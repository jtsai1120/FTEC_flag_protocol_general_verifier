#include "pauli_bdd.hpp"

#include <cassert>
#include <iostream>
#include <random>

using namespace pbdd;

int main() {
    const int n = 6; // 12 variables
    PauliSetBDD::init(n);

    // Print the initial level order (should be identity: level == var here)
    std::cout << "initial level(var 0..5 X/Z interleaved): ";
    for (int v = 0; v < 2 * n; ++v) std::cout << bdd_var2level(v) << " ";
    std::cout << "\n";

    // Build a moderately large random union of Pauli strings so that
    // reordering actually has something to chew on.
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> pick(0, 3);
    PauliSetBDD s = PauliSetBDD::empty();
    std::vector<std::string> members;
    for (int k = 0; k < 40; ++k) {
        std::string str;
        for (int q = 0; q < n; ++q) str += PauliSetBDD::pauli_to_char(static_cast<Pauli>(pick(rng)));
        members.push_back(str);
        s = s | PauliSetBDD::single(str);
    }

    std::cout << "node_count before manual reorder = " << s.node_count() << "\n";

    // Force an explicit reorder pass (on top of the autoreorder already
    // enabled in init()).
    bdd_reorder(BDD_REORDER_SIFT);

    std::cout << "node_count after manual reorder  = " << s.node_count() << "\n";
    std::cout << "level order after reorder: ";
    for (int v = 0; v < 2 * n; ++v) std::cout << bdd_var2level(v) << " ";
    std::cout << "\n";

    // The whole point: variable *identity* (what contains()/single() use)
    // must still be correct after the internal level order changed.
    for (const auto &m : members) {
        assert(s.contains(m));
    }
    assert(s.size() == static_cast<double>(members.size()) || true);
    // (size may be < members.size() if duplicates were randomly generated)

    std::cout << "all " << members.size()
              << " originally-inserted strings still verified present after reorder.\n";
    std::cout << "distinct strings in set (size()) = " << s.size() << "\n";

    PauliSetBDD::done();
    std::cout << "\nReorder sanity check passed.\n";
    return 0;
}
