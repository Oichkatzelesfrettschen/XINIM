#include "constant_time.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <span>

/**
 * @brief Unit test for pqcrypto::constant_time_equal.
 */
int main() {
    std::array<std::byte, 16> a{};
    std::array<std::byte, 16> b{};
    std::array<std::byte, 16> c{};

    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = std::byte{static_cast<unsigned char>(i)};
        b[i] = a[i];
        c[i] = std::byte{static_cast<unsigned char>(i + 1)};
    }

    assert(pqcrypto::constant_time_equal(a, b));
    assert(!pqcrypto::constant_time_equal(a, c));
    std::array<std::byte, 15> d{};
    assert(!pqcrypto::constant_time_equal(std::span{a.data(), 15}, d));
    return 0;
}
