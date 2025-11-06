/**
 * @file cpu_i386.cpp
 * @brief i386 CPU HAL Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/hal/cpu.hpp>
#include <stdint.h>

namespace xinim::hal {

/**
 * @brief i386 (32-bit x86) CPU implementation
 * 
 * This implementation provides CPU-specific operations for 32-bit x86 processors.
 * Compatible with i386, i486, Pentium, and later 32-bit x86 CPUs.
 */
class CpuI386 final : public Cpu {
  public:
    /**
     * @brief Execute CPUID instruction
     * @param leaf CPUID function (EAX input)
     * @param subleaf CPUID subfunction (ECX input)
     * @return CPUID result registers
     */
    CpuId cpuid(uint32_t leaf, uint32_t subleaf) noexcept override {
        CpuId id{};
#if defined(__i386__) || defined(_M_IX86)
        uint32_t a, b, c, d;
        // i386 CPUID - save EBX for PIC compatibility
        __asm__ volatile(
            "pushl %%ebx\n\t"
            "cpuid\n\t"
            "movl %%ebx, %1\n\t"
            "popl %%ebx\n\t"
            : "=a"(a), "=r"(b), "=c"(c), "=d"(d)
            : "a"(leaf), "c"(subleaf)
            : "memory"
        );
        id.eax = a;
        id.ebx = b;
        id.ecx = c;
        id.edx = d;
#else
        (void)leaf;
        (void)subleaf;
#endif
        return id;
    }

    /**
     * @brief Pause CPU execution briefly
     */
    void pause() noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        __asm__ volatile("pause");
#endif
    }

    /**
     * @brief Read Time Stamp Counter
     * @return 64-bit timestamp counter value
     */
    uint64_t rdtsc() noexcept override {
        uint32_t lo = 0, hi = 0;
#if defined(__i386__) || defined(_M_IX86)
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
#else
        (void)lo;
        (void)hi;
#endif
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }

    /**
     * @brief Enable CPU interrupts
     */
    void enable_interrupts() noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        __asm__ volatile("sti");
#endif
    }

    /**
     * @brief Disable CPU interrupts
     */
    void disable_interrupts() noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        __asm__ volatile("cli");
#endif
    }

    /**
     * @brief Halt CPU until next interrupt
     */
    void halt() noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        __asm__ volatile("hlt");
#endif
    }

    /**
     * @brief Invalidate TLB entry for given address
     * @param addr Virtual address to invalidate
     */
    void invlpg(uintptr_t addr) noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
#else
        (void)addr;
#endif
    }

    /**
     * @brief Read CR3 (page directory base register)
     * @return CR3 register value
     */
    uint64_t read_cr3() noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        uint32_t cr3;
        __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
        return static_cast<uint64_t>(cr3);
#else
        return 0;
#endif
    }

    /**
     * @brief Write CR3 (page directory base register)
     * @param value New CR3 value
     */
    void write_cr3(uint64_t value) noexcept override {
#if defined(__i386__) || defined(_M_IX86)
        uint32_t cr3 = static_cast<uint32_t>(value);
        __asm__ volatile("movl %0, %%cr3" : : "r"(cr3) : "memory");
#else
        (void)value;
#endif
    }
};

} // namespace xinim::hal
