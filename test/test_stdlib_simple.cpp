// Simple test for C++23 libc++ with C17 libc compatibility
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <ranges>
#include <span>
#include <filesystem>

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAILED: " #cond " at line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while(0)

int main() {
    std::cout << "Testing C++23/C17 Interoperability\n";
    std::cout << "===================================\n\n";
    
    // Test 1: C++23 string_view with C strings
    {
        std::cout << "Test 1: string_view with C strings... ";
        const char* c_str = "Hello, World!";
        std::string_view sv(c_str);
        ASSERT(sv.length() == std::strlen(c_str));
        ASSERT(sv == c_str);
        std::cout << "PASSED\n";
    }
    
    // Test 2: C++23 ranges with C arrays
    {
        std::cout << "Test 2: ranges with C arrays... ";
        int c_array[] = {5, 2, 8, 1, 9};
        std::ranges::sort(c_array);
        ASSERT(std::ranges::is_sorted(c_array));
        std::cout << "PASSED\n";
    }
    
    // Test 3: C++23 span with malloc'd memory
    {
        std::cout << "Test 3: span with malloc'd memory... ";
        int* data = static_cast<int*>(std::malloc(5 * sizeof(int)));
        for (int i = 0; i < 5; ++i) data[i] = i;
        
        std::span<int> sp(data, 5);
        ASSERT(sp.size() == 5);
        ASSERT(sp[2] == 2);
        
        std::free(data);
        std::cout << "PASSED\n";
    }
    
    // Test 4: Mixed FILE* and filesystem
    {
        std::cout << "Test 4: FILE* with filesystem... ";
        const char* filename = "test_temp.txt";
        
        // Write with C
        FILE* fp = std::fopen(filename, "w");
        ASSERT(fp != nullptr);
        std::fprintf(fp, "Test");
        std::fclose(fp);
        
        // Check with C++23
        ASSERT(std::filesystem::exists(filename));
        ASSERT(std::filesystem::file_size(filename) == 4);
        
        // Clean up
        std::filesystem::remove(filename);
        std::cout << "PASSED\n";
    }
    
    // Test 5: C++23 algorithms with C strings
    {
        std::cout << "Test 5: algorithms with C strings... ";
        char str[] = "hello";
        std::ranges::transform(str, str, [](char c) { 
            return std::toupper(c); 
        });
        ASSERT(std::strcmp(str, "HELLO") == 0);
        std::cout << "PASSED\n";
    }
    
    // Test 6: Vector with C array initialization
    {
        std::cout << "Test 6: vector from C array... ";
        int c_data[] = {1, 2, 3, 4, 5};
        std::vector<int> vec(std::begin(c_data), std::end(c_data));
        ASSERT(vec.size() == 5);
        ASSERT(vec[2] == 3);
        std::cout << "PASSED\n";
    }
    
    // Test 7: C memory functions with C++ objects
    {
        std::cout << "Test 7: memcpy with C++ objects... ";
        struct Simple { int a, b; };
        Simple s1{10, 20};
        Simple s2;
        std::memcpy(&s2, &s1, sizeof(Simple));
        ASSERT(s2.a == 10 && s2.b == 20);
        std::cout << "PASSED\n";
    }
    
    // Test 8: Library detection
    {
        std::cout << "\nLibrary Configuration:\n";
        #ifdef _LIBCPP_VERSION
            std::cout << "  Using libc++ version " << _LIBCPP_VERSION << "\n";
        #elif defined(__GLIBCXX__)
            std::cout << "  Using libstdc++ version " << __GLIBCXX__ << "\n";
        #else
            std::cout << "  Using unknown C++ standard library\n";
        #endif
        
        std::cout << "  C++ Standard: " << __cplusplus << "\n";
        
        #if __cpp_lib_ranges >= 202202L
            std::cout << "  C++23 Ranges: Available\n";
        #else
            std::cout << "  C++23 Ranges: Partial\n";
        #endif
    }
    
    std::cout << "\n===================================\n";
    std::cout << "All tests PASSED!\n";
    std::cout << "C++23 libc++ with C17 libc compatibility confirmed.\n";
    
    return 0;
}