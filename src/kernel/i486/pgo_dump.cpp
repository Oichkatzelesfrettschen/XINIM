// PGO profile dump + libclang_rt.profile libc stubs for bare-metal i486.
// WHY: The Clang profile runtime was built for Linux user-space; a bare-metal
// kernel cannot satisfy its file-I/O and threading dependencies.  We provide
// weak stubs so the linker is satisfied, then call __llvm_profile_write_buffer
// directly and stream the result to COM1 serial for host-side extraction.

#ifdef XINIM_PGO_DUMP_ENABLED

#include "pgo_dump.hpp"
#include <stdint.h>

// ============================================================
// LLVM profile runtime API (from compiler-rt headers)
// ============================================================
extern "C" {
    uint64_t __llvm_profile_get_size_for_buffer(void);
    int      __llvm_profile_write_buffer(char* Buffer);
    void     __llvm_profile_reset_counters(void);
}

// ============================================================
// COM1 serial helpers (direct port I/O, no OS)
// ============================================================
namespace {

static void com1_wait_ready() noexcept {
    uint8_t status;
    do {
        __asm__ volatile("inb $0x3FD, %0" : "=a"(status));
    } while (!(status & 0x20U));
}

static void com1_byte(char c) noexcept {
    com1_wait_ready();
    __asm__ volatile("outb %0, $0x3F8" :: "a"(static_cast<uint8_t>(c)));
}

static void com1_str(const char* s) noexcept {
    while (*s) com1_byte(*s++);
}

static void com1_hex8(uint32_t v) noexcept {
    static const char hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; --i)
        com1_byte(hex[(v >> (i * 4)) & 0xFU]);
}

static void com1_hex_byte(uint8_t b) noexcept {
    static const char hex[] = "0123456789abcdef";
    com1_byte(hex[b >> 4]);
    com1_byte(hex[b & 0xFU]);
}

// Profile data buffer.  512 KiB in BSS; covers typical kernel profiles.
// The kernel has 32 MiB of RAM so this is well within budget.
static char s_pgo_buf[512 * 1024];

} // namespace

// ============================================================
// Public dump entry point
// ============================================================
namespace xinim::i486::pgo {

void dump_profile_via_serial() noexcept {
    uint64_t sz = __llvm_profile_get_size_for_buffer();
    if (sz == 0U || sz > sizeof(s_pgo_buf)) {
        com1_str("\nXNPGO_ERR:size=");
        com1_hex8(static_cast<uint32_t>(sz >> 32));
        com1_hex8(static_cast<uint32_t>(sz));
        com1_byte('\n');
        return;
    }

    if (__llvm_profile_write_buffer(s_pgo_buf) != 0) {
        com1_str("\nXNPGO_ERR:write_buffer_failed\n");
        return;
    }

    // Frame: XNPGO_START:<8 hex digits of size (low 32 bits)>\n
    //        <hex pairs, 64 bytes per line>
    //        XNPGO_END\n
    com1_str("\nXNPGO_START:");
    com1_hex8(static_cast<uint32_t>(sz));
    com1_byte('\n');

    const uint8_t* data = reinterpret_cast<const uint8_t*>(s_pgo_buf);
    for (uint32_t i = 0U; i < static_cast<uint32_t>(sz); ++i) {
        com1_hex_byte(data[i]);
        // 64 bytes per line (128 hex chars) keeps lines short for log parsers
        if ((i & 63U) == 63U)
            com1_byte('\n');
    }
    com1_byte('\n');
    com1_str("XNPGO_END\n");
}

} // namespace xinim::i486::pgo

// ============================================================
// Weak stubs for libclang_rt.profile-i386.a OS dependencies.
// WHY: The archive contains TUs for file-backed profile writing that we
// never call, but the linker requires all referenced symbols to resolve.
// All stubs return failure/null/0 so that any accidental call is a no-op.
// ============================================================
extern "C" {

// Memory allocation -- used by value-profiling paths we don't exercise
__attribute__((weak)) void* malloc(unsigned long) { return nullptr; }
__attribute__((weak)) void* calloc(unsigned long, unsigned long) { return nullptr; }
__attribute__((weak)) void* realloc(void*, unsigned long) { return nullptr; }
__attribute__((weak)) void  free(void*) {}

// String / memory -- provide real implementations so write_buffer works
__attribute__((weak)) unsigned long strlen(const char* s) {
    unsigned long n = 0; while (s[n]) ++n; return n;
}
__attribute__((weak)) void* memcpy(void* dst, const void* src, unsigned long n) {
    auto* d = static_cast<uint8_t*>(dst);
    const auto* s = static_cast<const uint8_t*>(src);
    for (unsigned long i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
__attribute__((weak)) void* memset(void* dst, int c, unsigned long n) {
    auto* d = static_cast<uint8_t*>(dst);
    for (unsigned long i = 0; i < n; ++i) d[i] = static_cast<uint8_t>(c);
    return dst;
}
__attribute__((weak)) int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}
__attribute__((weak)) char* strchr(const char* s, int c) {
    while (*s && *s != static_cast<char>(c)) ++s;
    return *s == static_cast<char>(c) ? const_cast<char*>(s) : nullptr;
}
__attribute__((weak)) char* strrchr(const char* s, int c) {
    const char* last = nullptr;
    while (*s) { if (*s == static_cast<char>(c)) last = s; ++s; }
    return const_cast<char*>(last);
}
__attribute__((weak)) char* strncpy(char* dst, const char* src, unsigned long n) {
    unsigned long i = 0;
    while (i < n && src[i]) { dst[i] = src[i]; ++i; }
    while (i < n) dst[i++] = '\0';
    return dst;
}
__attribute__((weak)) char* strdup(const char*) { return nullptr; }
__attribute__((weak)) char* strerror(int) { return const_cast<char*>(""); }

// File I/O -- all stubs return failure; we never call the file-path
typedef struct { int _fd; } XFILE;
__attribute__((weak)) void*  fopen(const char*, const char*) { return nullptr; }
__attribute__((weak)) int    fclose(void*) { return -1; }
__attribute__((weak)) unsigned long fread(void*, unsigned long, unsigned long, void*) { return 0; }
__attribute__((weak)) unsigned long fwrite(const void*, unsigned long, unsigned long, void*) { return 0; }
__attribute__((weak)) int    fseek(void*, long, int) { return -1; }
__attribute__((weak)) long   ftell(void*) { return -1; }
__attribute__((weak)) int    fflush(void*) { return -1; }
__attribute__((weak)) int    feof(void*) { return 1; }
__attribute__((weak)) int    fileno(void*) { return -1; }
__attribute__((weak)) void*  fdopen(int, const char*) { return nullptr; }
__attribute__((weak)) int    ftruncate(int, long long) { return -1; }
__attribute__((weak)) int    fcntl(int, int, ...) { return -1; }
__attribute__((weak)) int    open(const char*, int, ...) { return -1; }
__attribute__((weak)) int    mkdir(const char*, unsigned int) { return -1; }

// Process / environment
__attribute__((weak)) int         getpid() { return 1; }
__attribute__((weak)) int         getpagesize() { return 4096; }
__attribute__((weak)) char*       getenv(const char*) { return nullptr; }
__attribute__((weak)) int         setenv(const char*, const char*, int) { return -1; }
__attribute__((weak)) int         uname(void*) { return -1; }
__attribute__((weak)) int         fork() { return -1; }
__attribute__((weak)) int         prctl(int, unsigned long, ...) { return -1; }
__attribute__((weak)) int         atexit(void (*)(void)) { return 0; }

// Memory mapping
__attribute__((weak)) void* mmap(void*, unsigned long, int, int, int, long long) {
    return reinterpret_cast<void*>(-1L);
}
__attribute__((weak)) int munmap(void*, unsigned long) { return -1; }
__attribute__((weak)) int madvise(void*, unsigned long, int) { return -1; }

// Error handling
__attribute__((weak)) int* __errno_location() {
    static int _errno = 0; return &_errno;
}
__attribute__((weak)) void __stack_chk_fail_local() { __builtin_trap(); }

// stdio -- stderr is used for error reporting in the runtime
// Provide a sentinel non-null value so the runtime doesn't crash on null check
static int _stderr_placeholder = 0;
__attribute__((weak)) void* stderr = &_stderr_placeholder;

// Formatted output -- runtime uses these for diagnostics; make them no-ops
__attribute__((weak)) int __fprintf_chk(void*, int, const char*, ...) { return 0; }
__attribute__((weak)) int __snprintf_chk(char* s, unsigned long n, int, unsigned long,
                                          const char*, ...) {
    if (n > 0) s[0] = '\0'; return 0;
}
__attribute__((weak)) int __memcpy_chk(void* d, const void* s,
                                        unsigned long n, unsigned long) {
    return static_cast<int>(reinterpret_cast<unsigned long>(memcpy(d, s, n)));
}
__attribute__((weak)) void* __memset_chk(void* d, int c, unsigned long n,
                                          unsigned long) {
    return memset(d, c, n);
}
__attribute__((weak)) char* __strncpy_chk(char* d, const char* s,
                                           unsigned long n, unsigned long) {
    return strncpy(d, s, n);
}

// Integer division helper pulled in by some profile TUs
__attribute__((weak)) unsigned long long __umoddi3(unsigned long long a,
                                                    unsigned long long b) {
    if (b == 0ULL) return 0ULL;
    return a % b;
}
__attribute__((weak)) long __isoc23_strtol(const char*, char**, int) { return 0; }

// ELF header pointer used by the profile runtime to locate the build ID.
// For a bare-metal kernel we have no standard ELF header in the load image;
// provide a sentinel that makes the runtime's lookup return "not found".
extern "C" __attribute__((weak)) const char __ehdr_start[4] = { 0, 0, 0, 0 };

} // extern "C"

#endif // XINIM_PGO_DUMP_ENABLED
