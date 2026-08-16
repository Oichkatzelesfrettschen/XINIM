#include <cstdint>
#include <iostream>

extern "C" unsigned __int128 __udivti3(unsigned __int128 dividend,
                                       unsigned __int128 divisor) noexcept;
extern "C" unsigned __int128 __umodti3(unsigned __int128 dividend,
                                       unsigned __int128 divisor) noexcept;

namespace {

    using Uint128 = unsigned __int128;

    bool expect_equal(Uint128 actual, Uint128 expected, const char *description) {
        if (actual == expected) {
            return true;
        }
        std::cerr << "FAIL: " << description << '\n';
        return false;
    }

    bool verify_case(Uint128 dividend, Uint128 divisor, Uint128 expected_quotient,
                     Uint128 expected_remainder, const char *description) {
        const Uint128 quotient = __udivti3(dividend, divisor);
        const Uint128 remainder = __umodti3(dividend, divisor);
        bool passed = expect_equal(quotient, expected_quotient, description);
        passed &= expect_equal(remainder, expected_remainder, description);
        passed &= expect_equal((quotient * divisor) + remainder, dividend, description);
        return passed;
    }

} // namespace

int main() {
    bool passed = true;
    passed &= verify_case(0U, 1U, 0U, 0U, "zero dividend");
    passed &= verify_case(1U, 2U, 0U, 1U, "divisor larger than dividend");
    passed &= verify_case(100'000'000'000'000'000ULL, 10'000'000'000ULL, 10'000'000ULL, 0U,
                          "APIC calibration scale");

    const Uint128 high_bit = static_cast<Uint128>(1U) << 127U;
    const Uint128 high_bit_divided_by_three =
        (static_cast<Uint128>(0x2aaaaaaaaaaaaaaaULL) << 64U) | 0xaaaaaaaaaaaaaaaaULL;
    passed &= verify_case(high_bit, 3U, high_bit_divided_by_three, 2U, "high bit dividend");
    passed &= verify_case(~static_cast<Uint128>(0U), high_bit + 1U, 1U, high_bit - 2U,
                          "overflowing partial remainder");
    return passed ? 0 : 1;
}
