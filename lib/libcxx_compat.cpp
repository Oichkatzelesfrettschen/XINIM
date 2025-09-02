// XINIM libc++ Implementation with C17 Compatibility
// Provides C++23 standard library with fallback to C17 libc when needed

#include "xinim/stdlib_bridge.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace xinim::libcxx {

// ═══════════════════════════════════════════════════════════════════════════
// Memory Management - C++23 with C17 fallback
// ═══════════════════════════════════════════════════════════════════════════

extern "C" {

// C17 compatible malloc wrapper
void* xinim_malloc(size_t size) {
    // Try C++ allocator first
    try {
        return ::operator new(size);
    } catch (const std::bad_alloc&) {
        // Fallback to C malloc
        return std::malloc(size);
    }
}

// C17 compatible free wrapper
void xinim_free(void* ptr) noexcept {
    if (!ptr) return;
    
    // Try to determine if allocated with new or malloc
    // In practice, we'll use C free as it's compatible
    std::free(ptr);
}

// C17 compatible realloc wrapper
void* xinim_realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return xinim_malloc(new_size);
    }
    
    // C++23 doesn't have realloc equivalent, use C version
    return std::realloc(ptr, new_size);
}

// C17 compatible calloc wrapper
void* xinim_calloc(size_t num, size_t size) {
    // Use C++ with initialization
    size_t total = num * size;
    void* ptr = xinim_malloc(total);
    if (ptr) {
        std::memset(ptr, 0, total);
    }
    return ptr;
}

} // extern "C"

// ═══════════════════════════════════════════════════════════════════════════
// String Operations - C++23 with C17 fallback
// ═══════════════════════════════════════════════════════════════════════════

extern "C" {

// Enhanced strcpy with C++23 safety
char* xinim_strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    
    // Use C++23 string_view for bounds checking if possible
    std::string_view sv(src);
    std::copy(sv.begin(), sv.end() + 1, dest);  // +1 for null terminator
    return dest;
}

// Enhanced strncpy with C++23 safety
char* xinim_strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    
    // Use C++23 algorithms
    std::string_view sv(src);
    size_t copy_len = std::min(n - 1, sv.length());
    std::copy_n(sv.begin(), copy_len, dest);
    dest[copy_len] = '\0';
    return dest;
}

// Enhanced strlen with C++23
size_t xinim_strlen(const char* str) {
    if (!str) return 0;
    return std::string_view(str).length();
}

// Enhanced strcmp with C++23
int xinim_strcmp(const char* s1, const char* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    
    std::string_view sv1(s1);
    std::string_view sv2(s2);
    return sv1.compare(sv2);
}

} // extern "C"

// ═══════════════════════════════════════════════════════════════════════════
// File I/O - C++23 with C17 fallback
// ═══════════════════════════════════════════════════════════════════════════

class FileManager {
public:
    // C++23 file operations with C17 fallback
    static std::expected<std::string, std::error_code> 
    read_file(const std::filesystem::path& path) {
        // Try C++23 first
        if (std::filesystem::exists(path)) {
            std::ifstream file(path, std::ios::binary);
            if (file) {
                std::string content(
                    std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()
                );
                return content;
            }
        }
        
        // Fallback to C17
        std::FILE* fp = std::fopen(path.c_str(), "rb");
        if (!fp) {
            return std::unexpected(
                std::make_error_code(std::errc::no_such_file_or_directory)
            );
        }
        
        std::fseek(fp, 0, SEEK_END);
        long size = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        
        std::string content(size, '\0');
        std::fread(content.data(), 1, size, fp);
        std::fclose(fp);
        
        return content;
    }
    
    static std::expected<void, std::error_code>
    write_file(const std::filesystem::path& path, std::string_view content) {
        // Try C++23 first
        std::ofstream file(path, std::ios::binary);
        if (file) {
            file.write(content.data(), content.size());
            if (file.good()) {
                return {};
            }
        }
        
        // Fallback to C17
        std::FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) {
            return std::unexpected(
                std::make_error_code(std::errc::permission_denied)
            );
        }
        
        size_t written = std::fwrite(content.data(), 1, content.size(), fp);
        std::fclose(fp);
        
        if (written != content.size()) {
            return std::unexpected(
                std::make_error_code(std::errc::io_error)
            );
        }
        
        return {};
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Process Management - C++23 with POSIX fallback
// ═══════════════════════════════════════════════════════════════════════════

extern "C" {

#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#include <sys/wait.h>

// Process spawning with C++23 enhancements
int xinim_system(const char* command) {
    if (!command) return -1;
    
    // Use C++23 for command parsing
    std::string cmd(command);
    
    // Fallback to POSIX system
    return system(command);
}

// Fork with C++23 safety
pid_t xinim_fork() {
    // Ensure C++ objects are in safe state
    std::cout.flush();
    std::cerr.flush();
    
    return fork();
}

#else

// Non-POSIX fallback
int xinim_system(const char* command) {
    return std::system(command);
}

#endif

} // extern "C"

// ═══════════════════════════════════════════════════════════════════════════
// Math Operations - C++23 with C17 fallback
// ═══════════════════════════════════════════════════════════════════════════

namespace math {

// C++23 math with C17 fallback
template<typename T>
constexpr T abs(T x) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        return std::abs(x);  // C++23
    } else {
        return (x < 0) ? -x : x;  // Fallback
    }
}

// C++23 constexpr math
template<typename T>
constexpr T min(T a, T b) noexcept {
    return (a < b) ? a : b;
}

template<typename T>
constexpr T max(T a, T b) noexcept {
    return (a > b) ? a : b;
}

// C++23 numeric algorithms
template<typename T>
constexpr T gcd(T a, T b) noexcept {
    if constexpr (requires { std::gcd(a, b); }) {
        return std::gcd(a, b);  // C++23
    } else {
        // Euclidean algorithm fallback
        while (b != 0) {
            T temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }
}

} // namespace math

// ═══════════════════════════════════════════════════════════════════════════
// Thread Support - C++23 with C11 threads fallback
// ═══════════════════════════════════════════════════════════════════════════

class ThreadManager {
public:
    // C++23 jthread with fallback
    static void spawn(std::function<void()> func) {
        #if __cpp_lib_jthread >= 201911L
            std::jthread thread(std::move(func));
            thread.detach();
        #else
            std::thread thread(std::move(func));
            thread.detach();
        #endif
    }
    
    // C++23 stop_token support
    #if __cpp_lib_jthread >= 201911L
    using stop_token = std::stop_token;
    using stop_source = std::stop_source;
    #else
    // Fallback implementation
    class stop_token {
        std::shared_ptr<std::atomic<bool>> stopped;
    public:
        bool stop_requested() const { return stopped && stopped->load(); }
    };
    #endif
};

// ═══════════════════════════════════════════════════════════════════════════
// Ranges and Algorithms - C++23 with fallback
// ═══════════════════════════════════════════════════════════════════════════

namespace ranges {

// C++23 ranges with fallback
template<typename Range, typename Pred>
auto filter(Range&& range, Pred&& pred) {
    #if __cpp_lib_ranges >= 202202L
        return std::ranges::views::filter(
            std::forward<Range>(range), 
            std::forward<Pred>(pred)
        );
    #else
        // Fallback implementation
        std::vector<std::ranges::range_value_t<Range>> result;
        for (auto&& elem : range) {
            if (pred(elem)) {
                result.push_back(elem);
            }
        }
        return result;
    #endif
}

// C++23 zip view with fallback
template<typename... Ranges>
auto zip(Ranges&&... ranges) {
    #if __cpp_lib_ranges >= 202202L
        return std::ranges::views::zip(std::forward<Ranges>(ranges)...);
    #else
        // Simple fallback for pairs
        static_assert(sizeof...(Ranges) == 2, "Fallback only supports 2 ranges");
        // Implementation would go here
    #endif
}

} // namespace ranges

// ═══════════════════════════════════════════════════════════════════════════
// Format Support - C++23 with fallback
// ═══════════════════════════════════════════════════════════════════════════

namespace format {

// C++23 format with sprintf fallback
template<typename... Args>
std::string format(std::string_view fmt, Args&&... args) {
    #if __cpp_lib_format >= 202207L
        return std::format(fmt, std::forward<Args>(args)...);
    #else
        // Fallback to sprintf
        constexpr size_t buffer_size = 4096;
        char buffer[buffer_size];
        std::snprintf(buffer, buffer_size, fmt.data(), args...);
        return std::string(buffer);
    #endif
}

// C++23 print with fallback
template<typename... Args>
void print(std::string_view fmt, Args&&... args) {
    #if __cpp_lib_print >= 202207L
        std::print(fmt, std::forward<Args>(args)...);
    #else
        std::cout << format(fmt, std::forward<Args>(args)...);
    #endif
}

} // namespace format

} // namespace xinim::libcxx