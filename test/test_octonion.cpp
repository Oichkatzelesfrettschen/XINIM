/**
 * @file test_octonion.cpp
 * @brief Unit tests for lattice::Octonion algebra.
 */

#include "octonion.hpp"
#include <cassert>
#include <array>

int main() {
    using lattice::Octonion;

    // Default construction: all zeros
    Octonion zero;
    for (auto v : zero.comp) assert(v == 0);

    // Explicit construction
    Octonion a{{1, 2, 3, 4, 5, 6, 7, 8}};
    assert(a.comp[0] == 1);
    assert(a.comp[7] == 8);

    // to_bytes / from_bytes round-trip
    std::array<std::uint8_t, 32> bytes{};
    a.to_bytes(bytes);
    auto b = Octonion::from_bytes(bytes);
    for (int i = 0; i < 8; ++i) assert(a.comp[i] == b.comp[i]);

    // Multiplication by identity e0 = {1, 0, 0, 0, 0, 0, 0, 0}
    Octonion identity{{1, 0, 0, 0, 0, 0, 0, 0}};
    Octonion prod = identity * a;
    for (int i = 0; i < 8; ++i) assert(prod.comp[i] == a.comp[i]);

    // Conjugate: e0 unchanged, e1..e7 negated
    Octonion c = a.conjugate();
    assert(c.comp[0] == a.comp[0]);
    for (int i = 1; i < 8; ++i) {
        assert(c.comp[i] == static_cast<std::uint32_t>(-static_cast<int32_t>(a.comp[i])));
    }

    // Zero * anything = zero
    Octonion z = zero * a;
    for (auto v : z.comp) assert(v == 0);

    return 0;
}
