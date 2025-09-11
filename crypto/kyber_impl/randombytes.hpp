// Random bytes generator header
#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
namespace xinim::crypto {
    class RandomBytesGenerator;
}
#endif

// C-compatible interface
#ifdef __cplusplus
extern "C" {
#endif

void randombytes(uint8_t* out, size_t outlen);
void randombytes_deterministic(uint8_t* out, size_t outlen, uint64_t seed);

#ifdef __cplusplus
}
#endif