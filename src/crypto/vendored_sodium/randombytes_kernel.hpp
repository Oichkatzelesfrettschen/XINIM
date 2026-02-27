// Kernel-space random byte generation via RDRAND/RDSEED.
// Replaces libsodium's randombytes_buf for freestanding environments.
// Full CSPRNG implementation deferred to Phase 7 (P7-T06).

#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::crypto {

// Fill buffer with random bytes using x86 RDRAND instruction.
// Returns true on success, false if RDRAND is unavailable or fails.
bool kernel_random_bytes(void* buf, size_t len);

// Check if RDRAND is available on this CPU (CPUID leaf 1, ECX bit 30).
bool has_rdrand();

} // namespace xinim::crypto
