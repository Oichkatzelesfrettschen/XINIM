// Kernel-space random byte generation via RDRAND/RDSEED.
// Phase 7 (P7-T06) will add CSPRNG seeded by RDRAND.

#include "randombytes_kernel.hpp"

namespace xinim::crypto {

bool has_rdrand() {
    uint32_t ecx = 0;
    asm volatile("cpuid" : "=c"(ecx) : "a"(1), "c"(0) : "ebx", "edx");
    return (ecx >> 30) & 1;
}

bool kernel_random_bytes(void* buf, size_t len) {
    if (!has_rdrand()) {
        return false;
    }

    auto* dst = static_cast<uint8_t*>(buf);
    size_t i = 0;

    // Fill 8 bytes at a time using RDRAND
    while (i + 8 <= len) {
        uint64_t val = 0;
        unsigned char ok = 0;
        // Retry up to 10 times per Intel recommendation
        for (int retry = 0; retry < 10; ++retry) {
            asm volatile("rdrand %0; setc %1" : "=r"(val), "=qm"(ok));
            if (ok) break;
        }
        if (!ok) return false;

        __builtin_memcpy(dst + i, &val, 8);
        i += 8;
    }

    // Handle remaining bytes
    if (i < len) {
        uint64_t val = 0;
        unsigned char ok = 0;
        for (int retry = 0; retry < 10; ++retry) {
            asm volatile("rdrand %0; setc %1" : "=r"(val), "=qm"(ok));
            if (ok) break;
        }
        if (!ok) return false;

        __builtin_memcpy(dst + i, &val, len - i);
    }

    return true;
}

} // namespace xinim::crypto
