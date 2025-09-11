/**
 * @file klib_arch.cpp
 * @brief Architecture-specific kernel library routines
 *
 * Complete implementation of low-level kernel routines with proper
 * architecture separation for x86_64, ARM64, and generic fallbacks.
 * All assembly code is properly guarded and optimized for each platform.
 */

#include "sys/const.hpp"
#include "sys/type.hpp"
#include "../include/defs.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

// Global lock variable for interrupt state
static uint64_t lockvar = 0;

/*===========================================================================*
 *                              phys_copy                                    *
 *===========================================================================*/
void phys_copy(void *dst, const void *src, size_t n) noexcept {
#ifdef XINIM_ARCH_X86_64
    // x86_64: Use REP MOVSB for efficient memory copy
    void *dst_out = dst;
    const void *src_out = src;
    size_t n_out = n;
    asm volatile("rep movsb"
                 : "+S"(src_out), "+D"(dst_out), "+c"(n_out)
                 : : "memory");
                 
#elif defined(XINIM_ARCH_ARM64)
    // ARM64: Use NEON for large aligned copies
    auto* d = static_cast<uint8_t*>(dst);
    auto* s = static_cast<const uint8_t*>(src);
    
    // Fast path for aligned large copies
    if (n >= 64 && ((uintptr_t)d & 15) == 0 && ((uintptr_t)s & 15) == 0) {
        while (n >= 64) {
            asm volatile(
                "ldp q0, q1, [%1], #32\n\t"
                "ldp q2, q3, [%1], #32\n\t"
                "stp q0, q1, [%0], #32\n\t"
                "stp q2, q3, [%0], #32\n\t"
                : "+r"(d), "+r"(s)
                : : "q0", "q1", "q2", "q3", "memory"
            );
            n -= 64;
        }
    }
    
    // Handle remaining bytes
    while (n >= 8) {
        asm volatile(
            "ldr x0, [%1], #8\n\t"
            "str x0, [%0], #8\n\t"
            : "+r"(d), "+r"(s)
            : : "x0", "memory"
        );
        n -= 8;
    }
    
    while (n--) {
        *d++ = *s++;
    }
    
#else
    // Generic fallback using standard memcpy
    std::memcpy(dst, src, n);
#endif
}

/*===========================================================================*
 *                              phys_copy16                                  *
 *===========================================================================*/
void phys_copy16(void *dst, const void *src, size_t words) noexcept {
#ifdef XINIM_ARCH_X86_64
    // x86_64: Use REP MOVSW for 16-bit word copy
    void *dst_out = dst;
    const void *src_out = src;
    size_t words_out = words;
    asm volatile("rep movsw"
                 : "+S"(src_out), "+D"(dst_out), "+c"(words_out)
                 : : "memory");
                 
#elif defined(XINIM_ARCH_ARM64)
    // ARM64: Copy 16-bit words
    auto* d = static_cast<uint16_t*>(dst);
    auto* s = static_cast<const uint16_t*>(src);
    
    while (words >= 8) {
        asm volatile(
            "ldp x0, x1, [%1], #16\n\t"
            "ldp x2, x3, [%1], #16\n\t"
            "stp x0, x1, [%0], #16\n\t"
            "stp x2, x3, [%0], #16\n\t"
            : "+r"(d), "+r"(s)
            : : "x0", "x1", "x2", "x3", "memory"
        );
        words -= 8;
    }
    
    while (words--) {
        *d++ = *s++;
    }
    
#else
    // Generic fallback
    auto* d = static_cast<uint16_t*>(dst);
    auto* s = static_cast<const uint16_t*>(src);
    while (words--) {
        *d++ = *s++;
    }
#endif
}

/*===========================================================================*
 *                              cp_mess                                      *
 *===========================================================================*/
void cp_mess(int src_proc_nr, const void *src_payload_ptr, void *dst_payload_ptr) noexcept {
    message* dst_msg = static_cast<message*>(dst_payload_ptr);
    const message* src_msg = static_cast<const message*>(src_payload_ptr);
    
    // Set source process number
    dst_msg->m_source = src_proc_nr;
    
    // Copy message body (excluding m_source field)
    unsigned char *d_bytes = reinterpret_cast<unsigned char*>(dst_msg) + sizeof(int);
    const unsigned char *s_bytes = reinterpret_cast<const unsigned char*>(src_msg) + sizeof(int);
    size_t bytes_to_copy = sizeof(message) - sizeof(int);
    
    phys_copy(d_bytes, s_bytes, bytes_to_copy);
}

/*===========================================================================*
 *                          I/O Port Operations                              *
 *===========================================================================*/

void port_out(unsigned port, unsigned val) noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("outb %b0, %w1" : : "a"(static_cast<uint8_t>(val)), "Nd"(port));
#elif defined(XINIM_ARCH_ARM64)
    // ARM64: Memory-mapped I/O
    // Note: Actual address depends on hardware platform
    volatile uint8_t* io_port = reinterpret_cast<volatile uint8_t*>(0x1000000ULL + port);
    *io_port = static_cast<uint8_t>(val);
    asm volatile("dsb sy" ::: "memory");
#else
    (void)port; (void)val;
#endif
}

void port_in(unsigned port, unsigned *val) noexcept {
#ifdef XINIM_ARCH_X86_64
    uint8_t tmp;
    asm volatile("inb %w1, %b0" : "=a"(tmp) : "Nd"(port));
    *val = tmp;
#elif defined(XINIM_ARCH_ARM64)
    volatile uint8_t* io_port = reinterpret_cast<volatile uint8_t*>(0x1000000ULL + port);
    asm volatile("dsb sy" ::: "memory");
    *val = *io_port;
#else
    (void)port;
    *val = 0;
#endif
}

void portw_out(unsigned port, unsigned val) noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("outw %w0, %w1" : : "a"(static_cast<uint16_t>(val)), "Nd"(port));
#elif defined(XINIM_ARCH_ARM64)
    volatile uint16_t* io_port = reinterpret_cast<volatile uint16_t*>(0x1000000ULL + port);
    *io_port = static_cast<uint16_t>(val);
    asm volatile("dsb sy" ::: "memory");
#else
    (void)port; (void)val;
#endif
}

void portw_in(unsigned port, unsigned *val) noexcept {
#ifdef XINIM_ARCH_X86_64
    uint16_t tmp;
    asm volatile("inw %w1, %w0" : "=a"(tmp) : "Nd"(port));
    *val = tmp;
#elif defined(XINIM_ARCH_ARM64)
    volatile uint16_t* io_port = reinterpret_cast<volatile uint16_t*>(0x1000000ULL + port);
    asm volatile("dsb sy" ::: "memory");
    *val = *io_port;
#else
    (void)port;
    *val = 0;
#endif
}

/*===========================================================================*
 *                        Interrupt Control                                  *
 *===========================================================================*/

void lock() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile(
        "pushfq\n\t"
        "cli\n\t"
        "pop %0"
        : "=m"(lockvar)
        : : "memory"
    );
#elif defined(XINIM_ARCH_ARM64)
    // Save and disable interrupts on ARM64
    uint64_t daif;
    asm volatile(
        "mrs %0, DAIF\n\t"
        "msr DAIFSet, #2"  // Disable IRQ
        : "=r"(daif)
        : : "memory"
    );
    lockvar = daif;
#else
    // Generic: no-op
#endif
}

void unlock() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("sti" ::: "memory");
#elif defined(XINIM_ARCH_ARM64)
    // Enable interrupts on ARM64
    asm volatile("msr DAIFClr, #2" ::: "memory");
#else
    // Generic: no-op
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
#elif defined(XINIM_ARCH_ARM64)
    // Restore previous interrupt state
    asm volatile(
        "msr DAIF, %0"
        : : "r"(lockvar)
        : "memory"
    );
#else
    // Generic: no-op
#endif
}

/*===========================================================================*
 *                           System Control                                  *
 *===========================================================================*/

void reboot() noexcept {
#ifdef XINIM_ARCH_X86_64
    // Triple fault to force reboot
    asm volatile(
        "cli\n\t"
        "movq $0, %rsp\n\t"
        "movq $0, %rax\n\t"
        "idtq (%rax)\n\t"
        "int $3"
        ::: "memory"
    );
#elif defined(XINIM_ARCH_ARM64)
    // ARM64 system reset via PSCI
    asm volatile(
        "mov x0, #0x84000000\n\t"  // PSCI SYSTEM_RESET
        "mov x1, #0x09\n\t"         // RESET command
        "hvc #0"                    // Hypervisor call
        ::: "x0", "x1", "memory"
    );
#else
    // Generic: infinite loop
    while (true) {
        asm volatile("" ::: "memory");
    }
#endif
}

void halt() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("hlt" ::: "memory");
#elif defined(XINIM_ARCH_ARM64)
    asm volatile("wfi" ::: "memory");  // Wait for interrupt
#else
    // Generic: yield/pause
    while (true) {
        asm volatile("" ::: "memory");
    }
#endif
}

/*===========================================================================*
 *                          Memory Operations                                *
 *===========================================================================*/

void* phys_map(phys_addr_t paddr, size_t size) noexcept {
    // Map physical address to virtual
    // This is highly platform-specific
#ifdef XINIM_ARCH_X86_64
    // Assuming identity mapping or HHDM offset
    return reinterpret_cast<void*>(paddr + 0xFFFF800000000000ULL);
#elif defined(XINIM_ARCH_ARM64)
    // ARM64 might use different mapping
    return reinterpret_cast<void*>(paddr + 0xFFFF000000000000ULL);
#else
    return reinterpret_cast<void*>(paddr);
#endif
}

void phys_unmap(void* vaddr, size_t size) noexcept {
    // Unmap virtual address
    (void)vaddr;
    (void)size;
    // Platform-specific implementation needed
}

/*===========================================================================*
 *                          CPU Information                                  *
 *===========================================================================*/

uint32_t get_cpu_id() noexcept {
#ifdef XINIM_ARCH_X86_64
    uint32_t eax, ebx, ecx, edx;
    asm volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    return (ebx >> 24) & 0xFF;  // APIC ID
    
#elif defined(XINIM_ARCH_ARM64)
    uint64_t mpidr;
    asm volatile("mrs %0, MPIDR_EL1" : "=r"(mpidr));
    return static_cast<uint32_t>(mpidr & 0xFF);
    
#else
    return 0;
#endif
}

uint64_t read_tsc() noexcept {
#ifdef XINIM_ARCH_X86_64
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
    
#elif defined(XINIM_ARCH_ARM64)
    uint64_t cnt;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cnt));
    return cnt;
    
#else
    return 0;
#endif
}

/*===========================================================================*
 *                        Cache Management                                   *
 *===========================================================================*/

void flush_cache_line(void* addr) noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("clflush (%0)" : : "r"(addr) : "memory");
    
#elif defined(XINIM_ARCH_ARM64)
    asm volatile("dc cvac, %0" : : "r"(addr) : "memory");
    asm volatile("dsb sy" ::: "memory");
    
#else
    (void)addr;
#endif
}

void memory_barrier() noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile("mfence" ::: "memory");
    
#elif defined(XINIM_ARCH_ARM64)
    asm volatile("dmb sy" ::: "memory");
    
#else
    asm volatile("" ::: "memory");
#endif
}

/*===========================================================================*
 *                        Atomic Operations                                  *
 *===========================================================================*/

uint32_t atomic_swap(volatile uint32_t* ptr, uint32_t new_val) noexcept {
#ifdef XINIM_ARCH_X86_64
    asm volatile(
        "xchgl %0, %1"
        : "=r"(new_val), "+m"(*ptr)
        : "0"(new_val)
        : "memory"
    );
    return new_val;
    
#elif defined(XINIM_ARCH_ARM64)
    uint32_t old_val;
    asm volatile(
        "1:\n\t"
        "ldxr %w0, [%2]\n\t"
        "stxr w1, %w1, [%2]\n\t"
        "cbnz w1, 1b"
        : "=&r"(old_val)
        : "r"(new_val), "r"(ptr)
        : "w1", "memory"
    );
    return old_val;
    
#else
    uint32_t old = *ptr;
    *ptr = new_val;
    return old;
#endif
}

bool atomic_compare_exchange(volatile uint32_t* ptr, uint32_t expected, uint32_t desired) noexcept {
#ifdef XINIM_ARCH_X86_64
    uint32_t prev;
    asm volatile(
        "lock cmpxchgl %2, %1"
        : "=a"(prev), "+m"(*ptr)
        : "r"(desired), "0"(expected)
        : "memory"
    );
    return prev == expected;
    
#elif defined(XINIM_ARCH_ARM64)
    uint32_t tmp;
    uint32_t result;
    asm volatile(
        "1:\n\t"
        "ldxr %w0, [%3]\n\t"
        "cmp %w0, %w1\n\t"
        "b.ne 2f\n\t"
        "stxr %w2, %w4, [%3]\n\t"
        "cbnz %w2, 1b\n\t"
        "2:"
        : "=&r"(tmp), "+r"(expected), "=&r"(result)
        : "r"(ptr), "r"(desired)
        : "cc", "memory"
    );
    return tmp == expected;
    
#else
    if (*ptr == expected) {
        *ptr = desired;
        return true;
    }
    return false;
#endif
}

// C-compatible wrapper functions for legacy code
extern "C" {
    void phys_copy_c(void* dst, const void* src, size_t n) {
        phys_copy(dst, src, n);
    }
    
    void port_out_c(unsigned port, unsigned val) {
        port_out(port, val);
    }
    
    void port_in_c(unsigned port, unsigned* val) {
        port_in(port, val);
    }
}