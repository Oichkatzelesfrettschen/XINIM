# XINIM C++23 Style Guide

**Version:** 1.0  
**Status:** Official  
**Last Updated:** 2026-01-03  
**Related:** ADR-0002 (Unified Coding Standards)

---

## Quick Reference

```cpp
// File naming: lowercase_with_underscores.cpp
// memory_manager.cpp ✓
// MemoryManager.cpp ✗

// Classes: PascalCase
class MemoryManager {};              // ✓
class memory_manager {};             // ✗

// Functions/variables: snake_case
void allocate_memory(size_t size);   // ✓
void AllocateMemory(size_t size);    // ✗

// Constants: UPPER_CASE or kPascalCase
constexpr int MAX_PROCESSES = 256;   // ✓
constexpr int kMaxProcesses = 256;   // ✓ (also acceptable)

// Namespaces: lowercase
namespace xinim::kernel {}           // ✓
namespace XINIM {}                   // ✗
```

---

## 1. Error Handling - std::expected

**Rule:** Use `std::expected<T, Error>` for all error handling. No exceptions, no error codes.

```cpp
namespace xinim::error {
    enum class Category {
        Memory, IO, Permission, Hardware, Network
    };
    
    struct Error {
        Category category;
        int code;
        std::string_view message;
        std::source_location location = std::source_location::current();
    };
    
    template<typename T>
    using Result = std::expected<T, Error>;
}

// Example usage
auto read_file(std::string_view path) -> Result<std::vector<std::byte>> {
    if (!file_exists(path)) {
        return std::unexpected(Error{
            .category = Category::IO,
            .code = ENOENT,
            .message = "File not found"
        });
    }
    
    auto data = /* read file */;
    return data;
}

// Calling code
auto result = read_file("/etc/config");
if (!result) {
    auto& error = result.error();
    log_error(error.message, error.location);
    return;
}
auto& data = result.value();
```

**Forbidden:**
```cpp
// ✗ Don't use exceptions
void foo() {
    throw std::runtime_error("error");
}

// ✗ Don't use error codes
int read_file(const char* path, char* buffer) {
    if (!exists) return -1;  // Bad
    return 0;
}

// ✗ Don't use std::optional for errors
std::optional<Data> fetch_data() {
    if (error) return std::nullopt;  // Loses error information
}
```

---

## 2. Memory Management - RAII First

**Rule:** All resources must be managed by RAII. No raw owning pointers.

```cpp
// ✓ Ownership transfer
auto allocate_buffer(size_t size) -> std::unique_ptr<Buffer> {
    return std::make_unique<Buffer>(size);
}

// ✓ Shared ownership (rare - document why)
// Multiple subsystems need access to cache
std::shared_ptr<Cache> global_cache = std::make_shared<Cache>();

// ✓ No ownership (views)
void process(std::span<const std::byte> data) {
    // Data is borrowed, not owned
}

void log(std::string_view message) {
    // String is borrowed
}

// ✓ RAII wrapper for hardware resources
class DMABuffer {
public:
    DMABuffer(size_t size) : size_(size) {
        buffer_ = allocate_dma(size);
    }
    
    ~DMABuffer() {
        if (buffer_) deallocate_dma(buffer_);
    }
    
    // Non-copyable, movable
    DMABuffer(const DMABuffer&) = delete;
    DMABuffer& operator=(const DMABuffer&) = delete;
    DMABuffer(DMABuffer&&) noexcept = default;
    DMABuffer& operator=(DMABuffer&&) noexcept = default;
    
private:
    void* buffer_ = nullptr;
    size_t size_ = 0;
};
```

**Forbidden:**
```cpp
// ✗ Raw owning pointers
void* malloc(size_t size);  // Use std::unique_ptr
int* ptr = new int;         // Use std::make_unique
delete ptr;                 // Use RAII

// ✗ Manual resource management
FILE* f = fopen("file", "r");
// ... might leak if exception
fclose(f);

// ✓ RAII alternative
struct FileHandle {
    FILE* f = nullptr;
    FileHandle(const char* path) : f(fopen(path, "r")) {}
    ~FileHandle() { if (f) fclose(f); }
};
```

---

## 3. Naming Conventions

### 3.1 Files

```cpp
// lowercase_with_underscores
memory_manager.cpp      // ✓
memory_manager.hpp      // ✓
MemoryManager.cpp       // ✗
memoryManager.cpp       // ✗
```

### 3.2 Classes and Structs

```cpp
// PascalCase
class MemoryManager {};
struct ProcessInfo {};
enum class ErrorCode {};

// ✗ Wrong
class memory_manager {};
class memoryManager {};
```

### 3.3 Functions and Variables

```cpp
// snake_case (consistent with STL)
void allocate_memory(size_t size);
int process_count = 0;
std::vector<int> active_processes;

// ✗ Wrong
void AllocateMemory(size_t size);
int ProcessCount = 0;
void allocateMemory(size_t size);
```

### 3.4 Constants

```cpp
// UPPER_CASE or kPascalCase
constexpr int MAX_PROCESSES = 256;
constexpr size_t kPageSize = 4096;

// Both styles acceptable, be consistent within a file
// ✗ Wrong: mixing styles
constexpr int MAX_PROCESSES = 256;
constexpr int kMinProcesses = 1;  // Don't mix
```

### 3.5 Namespaces

```cpp
// lowercase, no underscores
namespace xinim {}
namespace xinim::kernel {}
namespace xinim::drivers::net {}

// ✗ Wrong
namespace XINIM {}
namespace Xinim {}
namespace xinim_kernel {}
```

### 3.6 Template Parameters

```cpp
// PascalCase for types, snake_case for values
template<typename T, typename Allocator>
class Vector {};

template<size_t buffer_size>
class StaticBuffer {};

// ✗ Wrong
template<typename t>  // Use T
template<size_t BUFFER_SIZE>  // Use buffer_size
```

---

## 4. Include Organization

**Order (enforced by clang-format):**

```cpp
// 1. Associated header (if applicable)
#include "memory_manager.hpp"

// 2. C system headers
#include <stdint.h>
#include <unistd.h>

// 3. C++ standard library (alphabetical)
#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

// 4. XINIM headers (alphabetical)
#include <xinim/kernel/process.hpp>
#include <xinim/kernel/types.hpp>
#include <xinim/util/assert.hpp>

// 5. Third-party libraries (alphabetical)
#include <sodium.h>
```

**Include Guards:**

```cpp
// Use #pragma once (simpler, faster)
#pragma once

namespace xinim::kernel {
// ...
}

// ✗ Don't use traditional include guards
// #ifndef MEMORY_MANAGER_HPP
// #define MEMORY_MANAGER_HPP
```

---

## 5. Code Structure

### 5.1 Function Length

**Guideline:** Maximum 50 lines. Refactor longer functions.

```cpp
// ✓ Good: focused function
auto process_request(Request req) -> Result<Response> {
    auto validated = validate_request(req);
    if (!validated) return std::unexpected(validated.error());
    
    auto processed = process_data(validated.value());
    if (!processed) return std::unexpected(processed.error());
    
    return create_response(processed.value());
}

// ✗ Bad: 200+ line function
// Refactor using helper functions or extract class
```

### 5.2 Class Design

```cpp
// ✓ Good: single responsibility
class FileReader {
public:
    explicit FileReader(std::string_view path);
    auto read() -> Result<std::vector<std::byte>>;
    
private:
    std::string path_;
    std::unique_ptr<FileHandle> handle_;
};

// ✗ Bad: multiple responsibilities
class FileManager {
    // Reading, writing, permissions, caching, networking...
    // Split into focused classes
};
```

---

## 6. Modern C++23 Features

### 6.1 Use These Features

```cpp
// std::expected for error handling
auto divide(int a, int b) -> std::expected<int, Error> {
    if (b == 0) return std::unexpected(Error{/*...*/});
    return a / b;
}

// std::span for views
void process(std::span<const int> data) {
    for (int value : data) { /* ... */ }
}

// std::string_view for string views
void log(std::string_view message) {
    // No copy, efficient
}

// if consteval for compile-time detection
constexpr int hash(std::string_view str) {
    if consteval {
        // Compile-time FNV hash
        return compile_time_hash(str);
    } else {
        // Runtime CRC32 with SIMD
        return runtime_hash(str);
    }
}

// constexpr for compile-time computation
constexpr auto calculate_size() {
    return 4096 * 8;
}

// Structured bindings
auto [key, value] = map.find(id);

// Range-based for with ranges
for (auto& item : data | std::views::filter(pred)) {
    // ...
}
```

### 6.2 Avoid These Patterns

```cpp
// ✗ Raw arrays
int buffer[256];  // Use std::array or std::vector

// ✗ C-style casts
int* p = (int*)ptr;  // Use static_cast, reinterpret_cast

// ✗ NULL
int* p = NULL;  // Use nullptr

// ✗ typedef
typedef int MyInt;  // Use 'using MyInt = int;'

// ✗ Manual loops where algorithms exist
for (int i = 0; i < size; ++i) {
    if (pred(arr[i])) { /* ... */ }
}
// Use: std::ranges::for_each(arr, pred);
```

---

## 7. Documentation

### 7.1 Function Documentation

```cpp
/**
 * @brief Allocates a DMA buffer for device communication
 * 
 * @param size Size of buffer in bytes (must be page-aligned)
 * @param constraints DMA address constraints
 * @return Expected<DMABuffer> Buffer handle or error
 * 
 * @note Buffer is automatically freed when DMABuffer is destroyed
 * @warning Must be called with interrupts disabled
 * 
 * @example
 * auto buffer = allocate_dma_buffer(4096, constraints);
 * if (!buffer) {
 *     handle_error(buffer.error());
 * }
 */
auto allocate_dma_buffer(size_t size, const DMAConstraints& constraints)
    -> Result<DMABuffer>;
```

### 7.2 Class Documentation

```cpp
/**
 * @class MemoryManager
 * @brief Manages physical and virtual memory allocation
 * 
 * The MemoryManager provides RAII-based memory allocation with
 * automatic deallocation and leak detection.
 * 
 * @thread_safety Thread-safe, uses internal locking
 * @performance O(log n) allocation, O(1) deallocation
 */
class MemoryManager {
    // ...
};
```

---

## 8. Testing

```cpp
// Use descriptive test names
TEST(MemoryManager, AllocatesPageAlignedMemory) {
    MemoryManager mm;
    auto buffer = mm.allocate(4096);
    ASSERT_TRUE(buffer);
    EXPECT_EQ(buffer->size(), 4096);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(buffer->data()) % 4096, 0);
}

// Test error paths
TEST(FileReader, ReturnsErrorForMissingFile) {
    FileReader reader("/nonexistent");
    auto result = reader.read();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().category, Category::IO);
    EXPECT_EQ(result.error().code, ENOENT);
}
```

---

## 9. Performance Guidelines

```cpp
// ✓ Pass by const reference for large objects
void process(const std::vector<int>& data);

// ✓ Pass by value for small objects (< 16 bytes)
void set_id(uint64_t id);

// ✓ Use std::move for ownership transfer
auto buffer = std::move(temp_buffer);

// ✓ Mark functions noexcept when they don't throw
void swap(T& a, T& b) noexcept {
    // ...
}

// ✓ Use constexpr for compile-time computation
constexpr uint32_t hash(std::string_view str) {
    // Computed at compile time
}
```

---

## 10. Enforcement

### 10.1 Pre-commit Hooks

Automated checks run before every commit:
- clang-format (formatting)
- Naming conventions
- Forbidden patterns (throw, new/delete, malloc/free)

### 10.2 CI/CD

Continuous integration enforces:
- All style checks pass
- No clang-tidy warnings
- Documentation coverage ≥ 80%
- Test coverage ≥ 70%

### 10.3 Code Review

Reviewers check for:
- Adherence to style guide
- Proper use of std::expected
- RAII for all resources
- Appropriate use of modern C++23

---

## 11. Examples

### 11.1 Complete Example (Good)

```cpp
#pragma once

#include <memory>
#include <span>
#include <string_view>
#include <expected>

namespace xinim::kernel {

/**
 * @brief Manages memory allocation for kernel subsystems
 */
class MemoryManager {
public:
    /**
     * @brief Allocates aligned memory region
     * @param size Size in bytes (must be multiple of alignment)
     * @param alignment Alignment requirement (power of 2)
     * @return Buffer or error
     */
    auto allocate(size_t size, size_t alignment = 16)
        -> Result<std::unique_ptr<Buffer>>;
    
    /**
     * @brief Gets current memory statistics
     * @return Statistics structure
     */
    [[nodiscard]] auto statistics() const noexcept -> MemoryStats;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xinim::kernel
```

### 11.2 Complete Example (Bad - Don't Do This)

```cpp
// ✗ No include guard
// ✗ Wrong namespace style
namespace XINIM {

// ✗ Poorly documented
// ✗ Wrong naming
class memoryManager {
public:
    // ✗ Raw pointer (should be unique_ptr)
    // ✗ Error code instead of expected
    // ✗ Wrong naming
    int AllocateMemory(size_t Size, void** Buffer);
    
    // ✗ Throws exception (should use expected)
    void FreeMemory(void* ptr) throw();
    
private:
    // ✗ Raw pointer (memory leak risk)
    void* internalBuffer;
};

}
```

---

## 12. Migration Guide

### Step 1: New Code

All new code must follow this guide 100%.

### Step 2: Modified Code

When modifying existing code:
- Fix style in modified functions
- Refactor to std::expected if touching error handling
- Convert to smart pointers if touching memory management

### Step 3: Systematic Refactoring

- Phase 1: High-complexity functions (CCN > 40)
- Phase 2: Security-critical code
- Phase 3: Remaining codebase

---

## Questions?

See:
- ADR-0002: docs/adr/0002-unified-coding-standards.md
- Architecture Plan: docs/ARCHITECTURAL_COHERENCE_PLAN.md
- Ask in #xinim-dev channel

---

**Last Updated:** 2026-01-03  
**Maintainer:** XINIM Core Team
