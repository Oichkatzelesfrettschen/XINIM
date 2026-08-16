/**
 * @file test_fano_multiply.cpp
 * @brief Unit tests for Fano-plane octonion multiplication.
 */

#include "fano_octonion.hpp"

#include <cassert>

int main() {
    using lattice::fano_multiply;
    using lattice::Octonion;

    // Identity multiplication: e0 * X = X
    Octonion identity{{1, 0, 0, 0, 0, 0, 0, 0}};
    Octonion a{{3, 5, 7, 11, 13, 17, 19, 23}};

    Octonion prod = fano_multiply(identity, a);
    for (std::size_t component_index = 0; component_index < a.comp.size(); ++component_index)
        assert(prod.comp[component_index] == a.comp[component_index]);

    // X * identity = X
    prod = fano_multiply(a, identity);
    for (std::size_t component_index = 0; component_index < a.comp.size(); ++component_index)
        assert(prod.comp[component_index] == a.comp[component_index]);

    // Zero * X = zero
    Octonion zero;
    prod = fano_multiply(zero, a);
    for (auto v : prod.comp)
        assert(v == 0);

    // e_i * e_i = -1 (comp[0] = -1, rest = 0, in uint32_t wrap)
    // Test e1 * e1
    Octonion e1{{0, 1, 0, 0, 0, 0, 0, 0}};
    prod = fano_multiply(e1, e1);
    // e1 * e1 = -e0 in octonion algebra
    assert(prod.comp[0] == static_cast<std::uint32_t>(-1));
    for (std::size_t component_index = 1; component_index < prod.comp.size(); ++component_index)
        assert(prod.comp[component_index] == 0);

    // Fano multiplication should agree with Cayley-Dickson for identity
    Octonion cd_prod = identity * a;
    Octonion fn_prod = fano_multiply(identity, a);
    for (std::size_t component_index = 0; component_index < cd_prod.comp.size(); ++component_index)
        assert(cd_prod.comp[component_index] == fn_prod.comp[component_index]);

    return 0;
}
