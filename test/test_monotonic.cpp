#include "../src/kernel/time/monotonic.hpp"

#include <cstdint>
#include <cstdio>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while (0)

namespace {

std::uint64_t fake_monotonic_a() { return 123456789ULL; }
std::uint64_t fake_monotonic_b() { return 987654321ULL; }

int test_monotonic_defaults_to_zero() {
    xinim::time::monotonic_install(nullptr);
    CHECK(xinim::time::monotonic_ns() == 0ULL);
    return 0;
}

int test_monotonic_uses_installed_provider() {
    xinim::time::monotonic_install(fake_monotonic_a);
    CHECK(xinim::time::monotonic_ns() == 123456789ULL);

    xinim::time::monotonic_install(fake_monotonic_b);
    CHECK(xinim::time::monotonic_ns() == 987654321ULL);
    return 0;
}

void run_test(const char* name, int (*fn)(), int& passed, int& failed) {
    if (fn() == 0) {
        std::printf("PASS: %s\n", name);
        ++passed;
    } else {
        std::printf("FAIL: %s\n", name);
        ++failed;
    }
}

}

int main() {
    int passed = 0;
    int failed = 0;
    run_test("monotonic_defaults_to_zero", test_monotonic_defaults_to_zero, passed, failed);
    run_test("monotonic_uses_installed_provider", test_monotonic_uses_installed_provider, passed, failed);
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
