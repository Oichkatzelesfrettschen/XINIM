// XINIM Standard Library Bridge
// Provides C++23 libc++ with C17 libc fallback compatibility

#ifndef XINIM_STDLIB_BRIDGE_HPP
#define XINIM_STDLIB_BRIDGE_HPP

// Feature detection macros
#ifdef __has_include
  #if __has_include(<version>)
    #include <version>
  #endif
#endif

// Detect C++23 features
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202211L
  #define XINIM_HAS_EXPECTED 1
#endif

#if defined(__cpp_lib_format) && __cpp_lib_format >= 202207L
  #define XINIM_HAS_FORMAT 1
#endif

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 202202L
  #define XINIM_HAS_RANGES_V2 1
#endif

#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
  #define XINIM_HAS_MDSPAN 1
#endif

// Detect standard library implementation
#if defined(_LIBCPP_VERSION)
  #define XINIM_STDLIB_LIBCXX 1
  #define XINIM_STDLIB_NAME "libc++"
#elif defined(__GLIBCXX__)
  #define XINIM_STDLIB_LIBSTDCXX 1
  #define XINIM_STDLIB_NAME "libstdc++"
#elif defined(_MSC_VER)
  #define XINIM_STDLIB_MSVC 1
  #define XINIM_STDLIB_NAME "MSVC STL"
#else
  #define XINIM_STDLIB_UNKNOWN 1
  #define XINIM_STDLIB_NAME "unknown"
#endif

// C++23 standard library headers (libc++)
#ifdef __cplusplus

// Core language support
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <version>
#include <limits>
#include <typeinfo>
#include <type_traits>
#include <concepts>
#include <compare>
#include <coroutine>
#include <source_location>

// Utilities
#include <utility>
#include <tuple>
#include <optional>
#include <variant>
#include <any>
#include <bitset>
#include <functional>
#include <memory>
#include <memory_resource>
#include <scoped_allocator>
#include <chrono>
#include <ratio>
#include <system_error>

// C++23 specific utilities
#ifdef XINIM_HAS_EXPECTED
  #include <expected>
#endif

// Strings and I/O
#include <string>
#include <string_view>
#include <charconv>
#include <format>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
// C++23 features (conditional)
#ifdef __has_include
  #if __has_include(<syncstream>)
    #include <syncstream>
  #endif
  #if __has_include(<spanstream>)
    #include <spanstream>
  #endif
  #if __has_include(<print>)
    #include <print>
  #endif
#endif

// Containers
#include <array>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <queue>
#include <span>

// C++23 containers (conditional)
#ifdef XINIM_HAS_MDSPAN
  #include <mdspan>
#endif
#ifdef __has_include
  #if __has_include(<flat_map>)
    #include <flat_map>
  #endif
  #if __has_include(<flat_set>)
    #include <flat_set>
  #endif
#endif

// Algorithms and ranges
#include <algorithm>
#include <ranges>
#include <numeric>
#include <iterator>
#include <execution>
#include <bit>

// Concurrency
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <semaphore>
#include <latch>
#include <barrier>
#include <future>
#include <atomic>
#include <stop_token>

// Filesystem
#include <filesystem>

// Regular expressions and localization
#include <regex>
#include <locale>
#include <codecvt>

// Math
#include <cmath>
#include <complex>
#include <valarray>
#include <random>
#include <numbers>

// C library headers (C++ified versions)
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cinttypes>
#include <clocale>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cuchar>
#include <cwchar>
#include <cwctype>

#endif // __cplusplus

// C17 compatibility layer (extern "C" when needed)
#ifdef __cplusplus
extern "C" {
#endif

// C17 standard library headers for fallback
#ifdef XINIM_ENABLE_C17_FALLBACK
  #include <assert.h>
  #include <ctype.h>
  #include <errno.h>
  #include <float.h>
  #include <limits.h>
  #include <locale.h>
  #include <math.h>
  #include <setjmp.h>
  #include <signal.h>
  #include <stdarg.h>
  #include <stddef.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <time.h>
  
  // C11/C17 specific
  #include <stdalign.h>
  #include <stdatomic.h>
  #include <stdbool.h>
  #include <stdint.h>
  #include <stdnoreturn.h>
  #include <tgmath.h>
  #include <threads.h>
  #include <uchar.h>
  
  // POSIX extensions if available
  #ifdef _POSIX_C_SOURCE
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <dirent.h>
    #include <pthread.h>
  #endif
#endif

#ifdef __cplusplus
} // extern "C"
#endif

// Namespace for standard library abstractions
namespace xinim::stdlib {

// Type aliases for interoperability
#ifdef __cplusplus

// C++23 types
using std::size_t;
using std::ptrdiff_t;
using std::nullptr_t;
using std::byte;

// String types
using std::string;
using std::string_view;
using std::u8string;
using std::u16string;
using std::u32string;
using std::wstring;

// Smart pointers
using std::unique_ptr;
using std::shared_ptr;
using std::weak_ptr;
using std::make_unique;
using std::make_shared;

// Containers
using std::vector;
using std::array;
using std::span;
using std::optional;
using std::variant;

// C++23 specific
#ifdef XINIM_HAS_EXPECTED
using std::expected;
using std::unexpected;
#else
// Fallback implementation for pre-C++23
template<typename T, typename E>
class expected {
    // Simplified fallback
    std::variant<T, E> storage;
public:
    constexpr bool has_value() const noexcept {
        return std::holds_alternative<T>(storage);
    }
    constexpr T& value() & { return std::get<T>(storage); }
    constexpr const T& value() const& { return std::get<T>(storage); }
    constexpr E& error() & { return std::get<E>(storage); }
    constexpr const E& error() const& { return std::get<E>(storage); }
};
#endif

// Utility functions for C/C++ interop
template<typename T>
[[nodiscard]] constexpr auto c_str(const T& str) {
    if constexpr (std::is_same_v<T, std::string>) {
        return str.c_str();
    } else if constexpr (std::is_same_v<T, std::string_view>) {
        return str.data();
    } else if constexpr (std::is_pointer_v<T>) {
        return str;
    } else {
        static_assert(sizeof(T) == 0, "Unsupported string type");
    }
}

// Memory management helpers
template<typename T>
struct c_deleter {
    void operator()(T* ptr) const noexcept {
        std::free(ptr);
    }
};

template<typename T>
using c_unique_ptr = std::unique_ptr<T, c_deleter<T>>;

// Make C-compatible unique_ptr
template<typename T>
[[nodiscard]] c_unique_ptr<T> make_c_unique(std::size_t n = 1) {
    return c_unique_ptr<T>(static_cast<T*>(std::malloc(sizeof(T) * n)));
}

// File operations bridge
class file_handle {
    std::variant<std::FILE*, std::fstream> handle;
    
public:
    file_handle(const char* filename, const char* mode) {
        // Try C++ first, fallback to C
        if (std::string_view(mode).find('b') != std::string_view::npos) {
            handle = std::fstream(filename, std::ios::binary | std::ios::in | std::ios::out);
        } else {
            handle = std::fopen(filename, mode);
        }
    }
    
    ~file_handle() {
        if (std::holds_alternative<std::FILE*>(handle)) {
            if (auto* f = std::get<std::FILE*>(handle)) {
                std::fclose(f);
            }
        }
    }
    
    bool is_open() const {
        return std::visit([](const auto& h) {
            using T = std::decay_t<decltype(h)>;
            if constexpr (std::is_same_v<T, std::FILE*>) {
                return h != nullptr;
            } else {
                return h.is_open();
            }
        }, handle);
    }
};

#endif // __cplusplus

} // namespace xinim::stdlib

#endif // XINIM_STDLIB_BRIDGE_HPP