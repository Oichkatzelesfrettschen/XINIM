/**
 * @file test_octonion.cpp
 * @brief Unit tests for lattice::Octonion algebra.
 */

#include "octonion.hpp"

#include <array>
#include <cassert>

int main() {
    using lattice::Octonion;

    // Default construction: all zeros
    Octonion zero;
    for (auto v : zero.comp)
        assert(v == 0);

    // Explicit construction
    Octonion a{{1, 2, 3, 4, 5, 6, 7, 8}};
    assert(a.comp[0] == 1);
    assert(a.comp[7] == 8);

    // to_bytes / from_bytes round-trip
    std::array<std::uint8_t, 32> bytes{};
    a.to_bytes(bytes);
    auto b = Octonion::from_bytes(bytes);
    for (std::size_t component_index = 0; component_index < a.comp.size(); ++component_index)
        assert(a.comp[component_index] == b.comp[component_index]);

    // Multiplication by identity e0 = {1, 0, 0, 0, 0, 0, 0, 0}
    Octonion identity{{1, 0, 0, 0, 0, 0, 0, 0}};
    Octonion prod = identity * a;
    for (std::size_t component_index = 0; component_index < a.comp.size(); ++component_index)
        assert(prod.comp[component_index] == a.comp[component_index]);

    // Conjugate: e0 unchanged, e1..e7 negated
    Octonion c = a.conjugate();
    assert(c.comp[0] == a.comp[0]);
    for (std::size_t component_index = 1; component_index < a.comp.size(); ++component_index) {
        assert(c.comp[component_index] ==
               static_cast<std::uint32_t>(-static_cast<int32_t>(a.comp[component_index])));
    }

    // Zero * anything = zero
    Octonion z = zero * a;
    for (auto v : z.comp)
        assert(v == 0);

    return 0;
}
