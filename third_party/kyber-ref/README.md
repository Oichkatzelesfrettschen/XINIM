# Kyber768 Reference Implementation (C)

Unmodified pq-crystals Kyber768 reference C implementation from:
https://github.com/pq-crystals/kyber/tree/main/ref

Used during v1.4.0 Phase A development to debug and validate the native C++
Kyber768 KEM port in `src/crypto/kyber_impl/`. The C++ port is the production
code; these files are archived for reference and regression testing only.

## License

Public domain / CC0. The pq-crystals Kyber reference implementation is released
into the public domain. See the upstream repository for details.

## Files

22 files: the complete Kyber768 ref/ directory plus `test_ref.c` (a minimal
enc/dec roundtrip driver written during debugging).

## Build

These files are NOT built by the Xinim CMake build system. To compile
standalone for comparison testing:

    cc -O2 -DKYBER_K=3 -o test_ref test_ref.c indcpa.c poly.c polyvec.c \
       ntt.c reduce.c cbd.c symmetric-shake.c fips202.c verify.c randombytes.c
    ./test_ref

## Status

Archived. The native C++ port in `src/crypto/kyber_impl/` passes all 5
KEM roundtrip tests and is the authoritative implementation.
