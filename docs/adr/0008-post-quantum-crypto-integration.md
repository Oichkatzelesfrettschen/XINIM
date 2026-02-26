# ADR 0008: Post-Quantum Cryptography Integration

**Date**: 2026-02-26
**Status**: Accepted
**Deciders**: Xinim development team

## Context

Xinim includes a partial Kyber512 post-quantum KEM implementation:

- `src/crypto/kyber_impl/` -- Reference Kyber C implementation (FIPS 202, NTT, etc.)
- `src/crypto/kyber.cpp` -- High-level encrypt/decrypt using Kyber512 KEM
- `src/crypto/kyber_cpp23_simd.hpp` -- C++23 SIMD-accelerated variant
- `src/kernel/pqcrypto.cpp` -- Kernel-side PQ crypto integration

## Current State

| Component | Status |
|-----------|--------|
| FIPS 202 (SHA3/SHAKE) | CORRECT -- NIST KAT passes (test_fips202) |
| NTT/invNTT | CORRECT -- round-trip verified (test_ntt_roundtrip) |
| Montgomery reduce | CORRECT -- identity verified (test_montgomery_reduce) |
| Kyber512 constants | CORRECT -- parameter values verified (test_kyber_e2e, test_kyber_constants) |
| Kyber512 keygen/encap/decap | INCOMPLETE -- kem.cpp, poly.cpp, indcpa.cpp missing |
| AEAD (DEM) | INSECURE -- XOR placeholder, no authentication |

## Decision

1. **Kyber512 is the target KEM** for Phase 7 full implementation.
2. **ChaCha20-Poly1305 is the target AEAD** to replace the XOR placeholder.
   - Rationale: ChaCha20 is constant-time by design; Poly1305 is simple to implement.
   - Alternative AES-256-GCM rejected: requires AES-NI or software table-based impl.
3. **The missing Kyber source files** (kem.cpp, poly.cpp, polyvec.cpp, indcpa.cpp,
   cbd.cpp, symmetric.cpp, verify.cpp) will be ported from the NIST reference C
   implementation in Phase 7.
4. **Freestanding constraint**: No OS-level random; use `src/crypto/kyber_impl/randombytes.cpp`
   which uses `SYS_getrandom` on Linux hosts and `RDRAND` in kernel context.

## Security Warning (active until Phase 7)

The current `encrypt()`/`decrypt()` in `src/crypto/kyber.cpp` are NOT secure:
- No message authentication
- XOR with truncated shared secret is stream-cipher-equivalent but unauthenticated
- A `#pragma message` compile warning is in place

## Consequences

- Phase 7 adds the missing Kyber reference source files
- Phase 7 implements ChaCha20-Poly1305 as the AEAD layer
- `test_kyber_e2e.cpp` will be extended to a full keygen+encap+decap+decrypt round-trip
- `src/crypto/kyber.cpp` encrypt/decrypt will be updated to use the AEAD
