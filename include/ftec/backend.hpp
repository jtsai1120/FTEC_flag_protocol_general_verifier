#pragma once

#include "ftec/dag.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ftec {

// What one SE invocation produced classically: the value of its cm register
// and, for a flagged SE, its cf register. These are what the protocol's
// branch conditions read as id_N.s and id_N.f.
struct Outcome {
    std::vector<bool> syndrome;
    std::vector<bool> flag;

    [[nodiscard]] std::uint64_t syndrome_value() const;
    [[nodiscard]] std::uint64_t flag_value() const;
    [[nodiscard]] std::string   to_string() const;   // "s=0101 f=1"
};

// Why a path is not protected: two errors it cannot tell apart whose product
// acts non-trivially on the encoded state.
struct Failure {
    int         fault_count = 0;
    std::string detail;      // backend-specific, e.g. the two Paulis
};

// A representation of "the set of error configurations still possible".
//
// The verifier owns the search; a backend owns how states are represented and
// how a circuit acts on them. Splitting on the measurement outcome lives here
// rather than in the driver because only the backend knows which outcomes are
// reachable -- that is exactly the information a decision diagram holds.
//
// States are handles, not values: a backend may keep them in whatever store it
// likes, and the driver only ever passes back handles it was given.
class Backend {
public:
    using StateId = std::size_t;

    virtual ~Backend() = default;

    // Called once, before any stepping. The fault budget comes from the code
    // distance, so a backend never has to work it out.
    virtual void begin(const fpdl::CodeSpec& code, int tau) = 0;

    // The state before anything has run: no faults, empty record.
    [[nodiscard]] virtual StateId initial_state() = 0;

    [[nodiscard]] virtual int fault_count(StateId) const = 0;

    // Run one SE and split on what it measured. Every returned state is
    // non-empty; an outcome that cannot occur is simply absent.
    [[nodiscard]] virtual std::vector<std::pair<Outcome, StateId>>
    step(StateId, const CircuitRef&) = 0;

    // Two states the driver decided are indistinguishable. Merging is not
    // optional: reaching the same node with the same record after t faults is
    // one situation however the faults were distributed, and treating those
    // separately would report failures that a decoder can in fact tell apart.
    [[nodiscard]] virtual StateId merge(const std::vector<StateId>&) = 0;

    // Does this state contain two errors whose product is a logical operator?
    [[nodiscard]] virtual std::optional<Failure> check(StateId) = 0;

    // For diagnostics only.
    [[nodiscard]] virtual std::string describe(StateId) const { return {}; }
};

using BackendPtr = std::unique_ptr<Backend>;

} // namespace ftec
