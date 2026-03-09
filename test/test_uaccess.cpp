#include "../src/kernel/uaccess.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while (0)

static int test_is_user_address_validation() {
    char buffer[16] {};
    const auto addr = reinterpret_cast<std::uintptr_t>(buffer);

    CHECK(xinim::kernel::is_user_address(addr, sizeof(buffer)));
    CHECK(!xinim::kernel::is_user_address(0, sizeof(buffer)));
    CHECK(!xinim::kernel::is_user_address(xinim::kernel::USER_SPACE_END - 1, 2));
    return 0;
}

static int test_copy_from_user() {
    char source[] = "hello";
    char dest[sizeof(source)] {};
    CHECK(xinim::kernel::copy_from_user(dest, reinterpret_cast<std::uintptr_t>(source), sizeof(source)) == 0);
    CHECK(std::strcmp(dest, "hello") == 0);
    return 0;
}

static int test_copy_to_user() {
    char dest[6] {};
    const char source[] = "world";
    CHECK(xinim::kernel::copy_to_user(reinterpret_cast<std::uintptr_t>(dest), source, sizeof(source)) == 0);
    CHECK(std::strcmp(dest, "world") == 0);
    return 0;
}

static int test_copy_string_from_user() {
    char source[] = "xash";
    char dest[16] {};
    CHECK(xinim::kernel::copy_string_from_user(dest, reinterpret_cast<std::uintptr_t>(source), sizeof(dest)) == 0);
    CHECK(std::strcmp(dest, "xash") == 0);

    char long_source[] = "toolong";
    char small_dest[4] {};
    CHECK(xinim::kernel::copy_string_from_user(small_dest, reinterpret_cast<std::uintptr_t>(long_source), sizeof(small_dest)) == -ENAMETOOLONG);
    CHECK(small_dest[sizeof(small_dest) - 1] == '\0');
    return 0;
}

static int test_strnlen_user() {
    char source[] = "abc";
    CHECK(xinim::kernel::strnlen_user(reinterpret_cast<std::uintptr_t>(source), 10) == 3);
    CHECK(xinim::kernel::strnlen_user(0, 10) == -EFAULT);
    return 0;
}

static void run_test(const char* name, int (*fn)(), int& passed, int& failed) {
    if (fn() == 0) {
        std::printf("PASS: %s\n", name);
        ++passed;
    } else {
        std::printf("FAIL: %s\n", name);
        ++failed;
    }
}

int main() {
    int passed = 0;
    int failed = 0;
    run_test("is_user_address_validation", test_is_user_address_validation, passed, failed);
    run_test("copy_from_user", test_copy_from_user, passed, failed);
    run_test("copy_to_user", test_copy_to_user, passed, failed);
    run_test("copy_string_from_user", test_copy_string_from_user, passed, failed);
    run_test("strnlen_user", test_strnlen_user, passed, failed);
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
