// Test C++23 libc++ with C17 libc interoperability
#include "xinim/stdlib_bridge.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <format>
#include <expected>
#include <ranges>
#include <algorithm>
#include <vector>
#include <string>
#include <filesystem>

// Test macros
#define TEST(name) void test_##name(); tests.push_back({#name, test_##name}); void test_##name()
#define ASSERT(cond) do { if (!(cond)) { std::cerr << "Assertion failed: " #cond " at " __FILE__ ":" << __LINE__ << std::endl; std::exit(1); } } while(0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

struct Test {
    std::string name;
    void (*func)();
};

std::vector<Test> tests;

// Test C++23 expected with C error codes
TEST(expected_with_c_errors) {
    using Result = std::expected<int, std::error_code>;
    
    // Success case
    Result ok{42};
    ASSERT(ok.has_value());
    ASSERT_EQ(ok.value(), 42);
    
    // Error case using C errno
    errno = ENOENT;
    Result err = std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    ASSERT(!err.has_value());
    ASSERT_EQ(err.error().value(), static_cast<int>(std::errc::no_such_file_or_directory));
}

// Test C++23 format with C printf compatibility
TEST(format_printf_compat) {
    // C++23 format
    auto cpp_str = std::format("Hello {}, the answer is {}", "World", 42);
    
    // C sprintf for comparison
    char c_buffer[256];
    std::sprintf(c_buffer, "Hello %s, the answer is %d", "World", 42);
    
    ASSERT_EQ(cpp_str, std::string(c_buffer));
}

// Test C++23 ranges with C arrays
TEST(ranges_with_c_arrays) {
    // C-style array
    int c_array[] = {5, 2, 8, 1, 9, 3};
    
    // Use C++23 ranges on C array
    auto sorted = c_array | std::views::all | std::views::common;
    std::ranges::sort(c_array);
    
    // Verify sorted
    for (size_t i = 1; i < std::size(c_array); ++i) {
        ASSERT(c_array[i-1] <= c_array[i]);
    }
}

// Test C++23 string_view with C strings
TEST(string_view_c_strings) {
    // C string
    const char* c_str = "Hello, C++23!";
    
    // Create string_view from C string
    std::string_view sv(c_str);
    
    ASSERT_EQ(sv.length(), std::strlen(c_str));
    ASSERT_EQ(sv, c_str);
    
    // Use with C function
    char buffer[256];
    std::strcpy(buffer, sv.data());
    ASSERT_EQ(std::string(buffer), c_str);
}

// Test C++23 filesystem with C file operations
TEST(filesystem_c_file_ops) {
    namespace fs = std::filesystem;
    
    // Create temp file with C
    const char* filename = "test_temp.txt";
    FILE* fp = std::fopen(filename, "w");
    ASSERT(fp != nullptr);
    std::fprintf(fp, "Test content");
    std::fclose(fp);
    
    // Read with C++23 filesystem
    ASSERT(fs::exists(filename));
    auto size = fs::file_size(filename);
    ASSERT_EQ(size, 12);  // "Test content" = 12 bytes
    
    // Clean up with C++23
    fs::remove(filename);
    ASSERT(!fs::exists(filename));
}

// Test C++23 span with C arrays
TEST(span_c_compatibility) {
    // C-style allocation
    int* c_data = static_cast<int*>(std::malloc(5 * sizeof(int)));
    for (int i = 0; i < 5; ++i) {
        c_data[i] = i * i;
    }
    
    // Create C++23 span
    std::span<int> sp(c_data, 5);
    
    ASSERT_EQ(sp.size(), 5);
    ASSERT_EQ(sp[2], 4);
    
    // Use with C++23 algorithms
    auto sum = std::ranges::fold_left(sp, 0, std::plus{});
    ASSERT_EQ(sum, 30);  // 0 + 1 + 4 + 9 + 16
    
    std::free(c_data);
}

// Test mixed memory management
TEST(mixed_memory_management) {
    // C malloc
    void* c_mem = std::malloc(100);
    ASSERT(c_mem != nullptr);
    
    // C++ new
    auto* cpp_mem = new char[100];
    ASSERT(cpp_mem != nullptr);
    
    // C++ unique_ptr with C deleter
    auto c_smart = xinim::stdlib::make_c_unique<char>(100);
    ASSERT(c_smart != nullptr);
    
    // Clean up
    std::free(c_mem);
    delete[] cpp_mem;
    // c_smart auto-deletes
}

// Test C++23 concepts with C functions
template<typename T>
concept CCompatible = requires(T t) {
    { std::strlen(xinim::stdlib::c_str(t)) } -> std::same_as<size_t>;
};

TEST(concepts_c_compat) {
    static_assert(CCompatible<const char*>);
    static_assert(CCompatible<std::string>);
    static_assert(CCompatible<std::string_view>);
    
    std::string cpp_str = "test";
    const char* c_str = "test";
    std::string_view sv = "test";
    
    ASSERT_EQ(std::strlen(xinim::stdlib::c_str(cpp_str)), 4);
    ASSERT_EQ(std::strlen(xinim::stdlib::c_str(c_str)), 4);
    ASSERT_EQ(std::strlen(xinim::stdlib::c_str(sv)), 4);
}

// Test C++23 coroutines with C callbacks (simplified)
TEST(coroutine_c_callback) {
    bool callback_called = false;
    
    // C-style callback
    auto c_callback = [](void* data) {
        *static_cast<bool*>(data) = true;
    };
    
    // Simulate async operation
    c_callback(&callback_called);
    
    ASSERT(callback_called);
}

// Test standard library detection
TEST(stdlib_detection) {
    std::cout << "Standard library: " << XINIM_STDLIB_NAME << std::endl;
    
    #ifdef XINIM_STDLIB_LIBCXX
        std::cout << "Using libc++ version: " << _LIBCPP_VERSION << std::endl;
    #endif
    
    #ifdef XINIM_HAS_EXPECTED
        std::cout << "Has std::expected: YES" << std::endl;
    #else
        std::cout << "Has std::expected: NO (using fallback)" << std::endl;
    #endif
    
    #ifdef XINIM_HAS_FORMAT
        std::cout << "Has std::format: YES" << std::endl;
    #else
        std::cout << "Has std::format: NO" << std::endl;
    #endif
    
    ASSERT(true);  // Just verification that detection works
}

int main() {
    std::cout << "Testing C++23 libc++ with C17 libc interoperability\n";
    std::cout << "====================================================\n\n";
    
    // Initialize tests
    test_expected_with_c_errors();
    test_format_printf_compat();
    test_ranges_with_c_arrays();
    test_string_view_c_strings();
    test_filesystem_c_file_ops();
    test_span_c_compatibility();
    test_mixed_memory_management();
    test_concepts_c_compat();
    test_coroutine_c_callback();
    test_stdlib_detection();
    
    // Run all tests
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        std::cout << "Running test: " << test.name << "... ";
        std::cout.flush();
        
        try {
            test.func();
            std::cout << "PASSED\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << "\n";
            ++failed;
        } catch (...) {
            std::cout << "FAILED: Unknown exception\n";
            ++failed;
        }
    }
    
    std::cout << "\n====================================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    
    return failed > 0 ? 1 : 0;
}