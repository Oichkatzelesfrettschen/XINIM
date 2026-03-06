/**
 * @file klib_arch.cpp
 * @brief Architecture-specific kernel library routines and libc stubs.
 */

#include "sys/const.hpp"
#include "sys/type.hpp"
#include "../include/defs.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include "heap.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

static uint64_t lockvar = 0;

// v1.2.0: 4 MB free-list heap replaces the 1 MB bump allocator.
static constexpr size_t KERNEL_HEAP_SIZE = 4 * 1024 * 1024;
static uint8_t kernel_heap[KERNEL_HEAP_SIZE]
    __attribute__((aligned(16)));
static bool heap_initialized = false;

static void ensure_heap_init() {
    if (!heap_initialized) {
        xinim::kernel::heap_init(kernel_heap, KERNEL_HEAP_SIZE);
        heap_initialized = true;
    }
}

extern "C" {
void* malloc(size_t size) {
    ensure_heap_init();
    return xinim::kernel::heap_alloc(static_cast<uint64_t>(size));
}

void free(void* ptr) {
    xinim::kernel::heap_free(ptr);
}

size_t kernel_heap_used() {
    return static_cast<size_t>(xinim::kernel::heap_used());
}
size_t kernel_heap_total() {
    return static_cast<size_t>(xinim::kernel::heap_total());
}
}

// C++ Runtime stubs (Must be outside extern "C")
void* operator new(size_t size) { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete[](void* ptr) noexcept { free(ptr); }
void operator delete(void* ptr, size_t) noexcept { free(ptr); }
void operator delete[](void* ptr, size_t) noexcept { free(ptr); }

extern "C" {

void* memset(void* s, int c, size_t n) {
    unsigned char* p = static_cast<unsigned char*>(s);
    while (n--) *p++ = static_cast<unsigned char>(c);
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    while (n--) *d++ = *s++;
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void __stack_chk_fail(void) {
    while(1) { asm volatile("hlt"); }
}

void* __dso_handle = nullptr;

void abort(void) {
    while(1) { asm volatile("hlt"); }
}

int* __errno_location(void) {
    static int local_errno;
    return &local_errno;
}

void __assert_fail(const char *, const char *, unsigned int, const char *) {
    while(1) { asm volatile("hlt"); }
}

// Unwind stubs
void _Unwind_Resume(void*) { abort(); }
void* __cxa_allocate_exception(size_t) { return malloc(128); }
void __cxa_throw(void*, void*, void*) { abort(); }
void __cxa_begin_catch(void*) {}
void __cxa_end_catch() {}
void __cxa_guard_acquire() {}
void __cxa_guard_release() {}
void __cxa_atexit() {}
void __cxa_thread_atexit() {}

void* mmap(void*, size_t, int, int, int, std::int64_t) { return (void*)-1; }
int munmap(void*, size_t) { return -1; }
int mprotect(void*, size_t, int) { return -1; }
int madvise(void*, size_t, int) { return -1; }
int mlock(const void*, size_t) { return -1; }
int munlock(const void*, size_t) { return -1; }
long sysconf(int) { return -1; }

void __explicit_bzero_chk(void* s, size_t len, size_t slen) {
    if (len > slen) abort();
    memset(s, 0, len);
}
void* __memcpy_chk(void* dest, const void* src, size_t len, size_t destlen) {
    if (len > destlen) abort();
    return memcpy(dest, src, len);
}
void* __memset_chk(void* s, int c, size_t n, size_t slen) {
    if (n > slen) abort();
    return memset(s, c, n);
}

// Math stubs
float ceilf(float x) { return (float)((int)x + (x > 0)); }
unsigned __int128 __udivti3(unsigned __int128 a, unsigned __int128 b) {
    if (b == 0) abort();
    return a / b;
}

void phys_copy(void *dst, const void *src, size_t n) noexcept {
#ifdef XINIM_ARCH_X86_64
    void *dst_out = dst;
    const void *src_out = src;
    size_t n_out = n;
    asm volatile("rep movsb"
                 : "+S"(src_out), "+D"(dst_out), "+c"(n_out)
                 : : "memory");
#else
    memcpy(dst, src, n);
#endif
}

void phys_copy16(void *dst, const void *src, size_t words) noexcept {
#ifdef XINIM_ARCH_X86_64
    void *dst_out = dst;
    const void *src_out = src;
    size_t words_out = words;
    asm volatile("rep movsw"
                 : "+S"(src_out), "+D"(dst_out), "+c"(words_out)
                 : : "memory");
#else
    auto* d = static_cast<uint16_t*>(dst);
    auto* s = static_cast<const uint16_t*>(src);
    while (words--) *d++ = *s++;
#endif
}

void cp_mess(int src_proc_nr, uint64_t, const void *src_payload, 
             uint64_t, void *dst_payload) noexcept {
    message* dst_msg = static_cast<message*>(dst_payload);
    const message* src_msg = static_cast<const message*>(src_payload);
    dst_msg->m_source = src_proc_nr;
    unsigned char *d_bytes = reinterpret_cast<unsigned char*>(dst_msg) + sizeof(int);
    const unsigned char *s_bytes = reinterpret_cast<const unsigned char*>(src_msg) + sizeof(int);
    size_t bytes_to_copy = sizeof(message) - sizeof(int);
    phys_copy(d_bytes, s_bytes, bytes_to_copy);
}

void port_out(unsigned port, unsigned val) noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("outb %b0, %w1" : : "a"(static_cast<uint8_t>(val)), "Nd"(port));
#endif
}

void port_in(unsigned port, unsigned *val) noexcept {
#ifdef XINIM_ARCH_X86_64
    uint8_t tmp;
    asm volatile("inb %w1, %b0" : "=a"(tmp) : "Nd"(port));
    *val = tmp;
#else
    *val = 0;
#endif
}

void portw_out(unsigned port, unsigned val) noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("outw %w0, %w1" : : "a"(static_cast<uint16_t>(val)), "Nd"(port));
#endif
}

void portw_in(unsigned port, unsigned *val) noexcept {
#ifdef XINIM_ARCH_X86_64
    uint16_t tmp;
    asm volatile("inw %w1, %w0" : "=a"(tmp) : "Nd"(port));
    *val = tmp;
#else
    (void)port; *val = 0;
#endif
}

void lock() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile(
        "pushfq\n\t"
        "cli\n\t"
        "pop %0"
        : "=m"(lockvar)
        : : "memory"
    );
#endif
}

void unlock() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("sti" ::: "memory");
#endif
}

void restore() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile(
        "push %0\n\t"
        "popfq"
        : : "m"(lockvar)
        : "memory"
    );
#endif
}

void reboot() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile(
        "cli\n\t"
        "movq $0, %%rsp\n\t"
        "movq $0, %%rax\n\t"
        "lidtq (%%rax)\n\t"
        "int $3"
        ::: "memory"
    );
#endif
}

void halt() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("hlt" ::: "memory");
#endif
}

} // extern "C"