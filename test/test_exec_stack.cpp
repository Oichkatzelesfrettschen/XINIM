#include "../src/kernel/exec_stack.hpp"

#include <cstdio>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while (0)

static int test_count_strings() {
    char arg0[] = "xash";
    char arg1[] = "-c";
    char* argv[] = {arg0, arg1, nullptr};
    CHECK(xinim::kernel::count_strings(argv) == 2);
    CHECK(xinim::kernel::count_strings(nullptr) == 0);
    return 0;
}

static int test_string_size() {
    char arg0[] = "xash";
    char arg1[] = "hello";
    char* argv[] = {arg0, arg1, nullptr};
    CHECK(xinim::kernel::calculate_string_size(argv) == 11);
    return 0;
}

static int test_stack_size_alignment() {
    const std::size_t size = xinim::kernel::calculate_stack_size(3, 2, 20, 12);
    CHECK(size % 16 == 0);
    CHECK(size >= sizeof(std::uint64_t));
    return 0;
}

static int test_stack_pointer_alignment() {
    char arg0[] = "xash";
    char arg1[] = "script.sh";
    char env0[] = "PATH=/bin";
    char* argv[] = {arg0, arg1, nullptr};
    char* envp[] = {env0, nullptr};

    const std::uint64_t stack_top = 0x80000000ULL;
    const std::uint64_t stack_ptr = xinim::kernel::setup_exec_stack(stack_top, argv, envp);
    CHECK(stack_ptr != 0);
    CHECK(stack_ptr < stack_top);
    CHECK((stack_ptr & 0xFULL) == 0);
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
    run_test("count_strings", test_count_strings, passed, failed);
    run_test("calculate_string_size", test_string_size, passed, failed);
    run_test("calculate_stack_size_alignment", test_stack_size_alignment, passed, failed);
    run_test("setup_exec_stack_alignment", test_stack_pointer_alignment, passed, failed);
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
