#pragma once
// CPUID feature detection for the i686 kernel.
// WHY: i686 (Pentium Pro+) adds APIC, CMOV, SSE, SYSENTER and other features
// that the i486 lane does not have.  We detect them at boot so that downstream
// initialisation code can branch without risking an illegal-instruction fault.
// Leaf 1 EDX bits per Intel SDM Vol. 2A Table 3-8.

#include <stdint.h>

namespace xinim::i686 {

struct CpuFeatures {
    bool has_fpu;      // EDX[0]  -- on-chip FPU
    bool has_tsc;      // EDX[4]  -- RDTSC
    bool has_msr;      // EDX[5]  -- RDMSR/WRMSR
    bool has_apic;     // EDX[9]  -- local APIC on-chip
    bool has_sysenter; // EDX[11] -- SYSENTER/SYSEXIT
    bool has_cmov;     // EDX[15] -- CMOV
    bool has_fxsr;     // EDX[24] -- FXSAVE/FXRESTORE
    bool has_sse;      // EDX[25] -- SSE1
    bool has_sse2;     // EDX[26] -- SSE2
};

// Populated once by initialize_cpuid() during early boot.
extern CpuFeatures g_cpu_features;

// Run CPUID leaf 1 and populate g_cpu_features.
void initialize_cpuid() noexcept;

// Read-only accessor.
[[nodiscard]] const CpuFeatures& cpu_features() noexcept;

} // namespace xinim::i686
