#include <cstdint>

extern "C" void abort(void);

namespace {

    using Uint128 = unsigned __int128;

    struct DivisionResult {
        Uint128 quotient;
        Uint128 remainder;
    };

    [[nodiscard]] DivisionResult divide_unsigned_128(Uint128 dividend, Uint128 divisor) noexcept {
        if (divisor == 0U) {
            abort();
        }

        Uint128 quotient = 0U;
        Uint128 remainder = 0U;
        constexpr unsigned kUint128Bits = 128U;

        // A division operator here would be lowered back into __udivti3 and recurse
        // until the kernel stack faults. Restoring binary division uses only shifts,
        // subtraction, and comparison, so the compiler-runtime entry is self-hosting.
        for (unsigned remaining_bits = kUint128Bits; remaining_bits > 0U; --remaining_bits) {
            const unsigned bit_index = remaining_bits - 1U;
            const bool remainder_overflow = (remainder >> (kUint128Bits - 1U)) != 0U;
            remainder = (remainder << 1U) | ((dividend >> bit_index) & 1U);
            if (remainder_overflow || remainder >= divisor) {
                remainder -= divisor;
                quotient |= static_cast<Uint128>(1U) << bit_index;
            }
        }

        return DivisionResult{quotient, remainder};
    }

} // namespace

extern "C" unsigned __int128 __udivti3(unsigned __int128 dividend,
                                       unsigned __int128 divisor) noexcept {
    return divide_unsigned_128(dividend, divisor).quotient;
}

extern "C" unsigned __int128 __umodti3(unsigned __int128 dividend,
                                       unsigned __int128 divisor) noexcept {
    return divide_unsigned_128(dividend, divisor).remainder;
}
