/**
 * @file test_fano_multiply.cpp
 * @brief Unit tests for Fano-plane octonion multiplication.
 */

#include "fano_octonion.hpp"
#include <cassert>

int main() {
    using lattice::Octonion;
    using lattice::fano_multiply;

    // Identity multiplication: e0 * X = X
    Octonion identity{{1, 0, 0, 0, 0, 0, 0, 0}};
    Octonion a{{3, 5, 7, 11, 13, 17, 19, 23}};

    Octonion prod = fano_multiply(identity, a);
    for (int i = 0; i < 8; ++i) assert(prod.comp[i] == a.comp[i]);

    // X * identity = X
    prod = fano_multiply(a, identity);
    for (int i = 0; i < 8; ++i) assert(prod.comp[i] == a.comp[i]);

    // Zero * X = zero
    Octonion zero;
    prod = fano_multiply(zero, a);
    for (auto v : prod.comp) assert(v == 0);

    // e_i * e_i = -1 (comp[0] = -1, rest = 0, in uint32_t wrap)
    // Test e1 * e1
    Octonion e1{{0, 1, 0, 0, 0, 0, 0, 0}};
    prod = fano_multiply(e1, e1);
    // e1 * e1 = -e0 in octonion algebra
    assert(prod.comp[0] == static_cast<std::uint32_t>(-1));
    for (int i = 1; i < 8; ++i) assert(prod.comp[i] == 0);

    // Fano multiplication should agree with Cayley-Dickson for identity
    Octonion cd_prod = identity * a;
    Octonion fn_prod = fano_multiply(identity, a);
    for (int i = 0; i < 8; ++i) assert(cd_prod.comp[i] == fn_prod.comp[i]);

    return 0;
}
