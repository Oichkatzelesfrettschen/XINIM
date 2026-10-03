#include <xinim/hal/cpu.hpp>
#include <stdint.h>

namespace xinim::hal {

class CpuX86_64 final : public Cpu {
  public:
    CpuId cpuid(uint32_t leaf, uint32_t subleaf) noexcept override {
        CpuId id{};
#if defined(__x86_64__) || defined(__i386__)
        uint32_t a, b, c, d;
        __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));
        id.eax = a; id.ebx = b; id.ecx = c; id.edx = d;
#else
        (void)leaf; (void)subleaf;
#endif
        return id;
    }
    void pause() noexcept override {
#if defined(__x86_64__) || defined(__i386__)
        __asm__ volatile("pause");
#endif
    }
    uint64_t rdtsc() noexcept override {
        uint32_t lo=0, hi=0; 
#if defined(__x86_64__) || defined(__i386__)
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
#else
        (void)lo; (void)hi;
#endif
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }
    void enable_interrupts() noexcept override { __asm__ volatile("sti"); }
    void disable_interrupts() noexcept override { __asm__ volatile("cli"); }
};

} // namespace xinim::hal
