#include "cpuid.hpp"

namespace xinim::i686 {

CpuFeatures g_cpu_features{};

void initialize_cpuid() noexcept {
    // Run CPUID leaf 1; EAX=1 -> EDX returns feature bits.
    uint32_t edx = 0U;
    __asm__ volatile(
        "movl $1, %%eax\n\t"
        "cpuid\n\t"
        : "=d"(edx)
        :
        : "eax", "ebx", "ecx"
    );

    g_cpu_features.has_fpu      = ((edx >> 0U)  & 1U) != 0U;
    g_cpu_features.has_tsc      = ((edx >> 4U)  & 1U) != 0U;
    g_cpu_features.has_msr      = ((edx >> 5U)  & 1U) != 0U;
    g_cpu_features.has_apic     = ((edx >> 9U)  & 1U) != 0U;
    g_cpu_features.has_sysenter = ((edx >> 11U) & 1U) != 0U;
    g_cpu_features.has_cmov     = ((edx >> 15U) & 1U) != 0U;
    g_cpu_features.has_fxsr     = ((edx >> 24U) & 1U) != 0U;
    g_cpu_features.has_sse      = ((edx >> 25U) & 1U) != 0U;
    g_cpu_features.has_sse2     = ((edx >> 26U) & 1U) != 0U;
}

const CpuFeatures& cpu_features() noexcept {
    return g_cpu_features;
}

} // namespace xinim::i686
