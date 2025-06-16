/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

// Modern C++23 header guard
#pragma once

// Use the C++23 versions of the system headers
#include "../h/callnr.hpp" // system call numbers
#include "../h/const.hpp"  // system-wide constants
#include "../h/error.hpp"  // error codes
#include "../h/type.hpp"   // basic MINIX types (includes xinim/core_types.hpp indirectly)
#include "defs.hpp"        // project specific definitions
#include <cstddef>         // for std::size_t, nullptr (though NIL_PTR is from h/const.hpp)
// #include "xinim/core_types.hpp" // Explicitly if needed, but type.hpp should suffice

extern message M;

// Indices for the message passing service queues
// Converted to inline constexpr values for type safety.
inline constexpr int MM = 0; // memory manager index
inline constexpr int FS = 1; // file system index

// Aliases kept for clarity in modern code
inline constexpr int kMM = MM;
inline constexpr int kFS = FS;

// System call interface (typically C linkage)
extern "C" int callm1(int proc, int syscallnr, int int1, int int2, int int3, char *ptr1, char *ptr2,
                      char *ptr3) noexcept;
extern "C" int callm3(int proc, int syscallnr, int int1,
                      const char *name) noexcept; // name -> const char*
extern "C" int callx(int proc, int syscallnr) noexcept;
extern "C" std::size_t len(const char *s) noexcept; // return int -> std::size_t, s -> const char*
extern "C" int send(int dst, message *m_ptr) noexcept;
extern "C" int receive(int src, message *m_ptr) noexcept;
extern "C" int sendrec(int srcdest, message *m_ptr) noexcept;
extern int errno;                 // Standard global
extern "C" int begsig() noexcept; /* interrupts all vector here, (void) -> () */

/* Memory allocation wrappers */
// Assuming these are C-linkage if used across C/C++ boundaries, and noexcept if they abort on
// failure
extern "C" void *safe_malloc(size_t size) noexcept;
extern "C" void safe_free(void *ptr) noexcept;

// RAII helper managing memory obtained through safe_malloc.
// Automatically frees the memory when going out of scope.
template <typename Type> class SafeBuffer {
  public:
    // Allocate space for 'count' objects of the given type using safe_malloc.
    explicit SafeBuffer(std::size_t count = 1)
        : size_(count), ptr_(static_cast<Type *>(safe_malloc(sizeof(Type) * count))) {}

    // Free the owned memory.
    ~SafeBuffer() { safe_free(ptr_); }

    // Non-copyable to avoid double free.
    SafeBuffer(const SafeBuffer &) = delete;
    SafeBuffer &operator=(const SafeBuffer &) = delete;

    // Move support for flexibility.
    SafeBuffer(SafeBuffer &&other) noexcept : size_(other.size_), ptr_(other.ptr_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }
    SafeBuffer &operator=(SafeBuffer &&other) noexcept {
        if (this != &other) {
            safe_free(ptr_);
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // Access the underlying pointer.
    [[nodiscard]] Type *get() noexcept { return ptr_; }
    [[nodiscard]] const Type *get() const noexcept { return ptr_; }

    // Array-style element access.
    Type &operator[](std::size_t idx) noexcept { return ptr_[idx]; }
    const Type &operator[](std::size_t idx) const noexcept { return ptr_[idx]; }

    // Release ownership without freeing.
    [[nodiscard]] Type *release() noexcept {
        Type *tmp = ptr_;
        ptr_ = nullptr;
        size_ = 0;
        return tmp;
    }

  private:
    std::size_t size_{}; // number of elements
    Type *ptr_{nullptr}; // managed pointer
};
