#include "time/apic_calibration_math.hpp"

#include <cstdint>
#include <iostream>

namespace {

    bool expect_equal(uint32_t actual, uint32_t expected, const char *description) {
        if (actual == expected) {
            return true;
        }
        std::cerr << "FAIL: " << description << ": expected " << expected << ", got " << actual
                  << '\n';
        return false;
    }

} // namespace

int main() {
    bool passed = true;
    passed &= expect_equal(
        xinim::time::compute_apic_timer_initial_count(100'000U, 10'000U, 10'000'000U, 100U),
        10'000'000U, "100 MHz APIC measured for 100 microseconds");
    passed &=
        expect_equal(xinim::time::compute_apic_timer_initial_count(0U, 10'000U, 10'000'000U, 100U),
                     0U, "zero APIC progress is rejected");
    passed &=
        expect_equal(xinim::time::compute_apic_timer_initial_count(100U, 0U, 10'000'000U, 100U), 0U,
                     "zero HPET progress is rejected");
    passed &= expect_equal(xinim::time::compute_apic_timer_initial_count(UINT32_MAX, 1U, 1U, 1U),
                           0U, "counts wider than the APIC register are rejected");
    return passed ? 0 : 1;
}
