/**
 * @file reduce.cpp
 * @brief Modular reduction implementations for Kyber.
 *
 * Provides the KYBER_NAMESPACE-prefixed montgomery_reduce and barrett_reduce
 * symbols declared in reduce.h.  The actual computation is delegated to the
 * inline templates in reduce.hpp (xinim::crypto::kyber namespace).
 *
 * reduce.h uses #define montgomery_reduce KYBER_NAMESPACE(montgomery_reduce)
 * which expands to e.g. pqcrystals_kyber768_ref_montgomery_reduce.
 * This translation unit defines those concrete symbols so poly.cpp and other
 * C-style callers can link against them without including reduce.hpp
 * (which would destroy the params.h macros via params.hpp undef).
 */

// Include params.h first so KYBER_NAMESPACE is defined as a macro.
#include "params.h"

// Include reduce.hpp for the inline template implementations.
// Note: params.hpp (pulled in by reduce.hpp) will #undef KYBER_POLYCOMPRESSEDBYTES
// and friends, but that is fine here since we only need the reduce functions.
#include "reduce.hpp"

using xinim::crypto::kyber::montgomery_reduce;
using xinim::crypto::kyber::barrett_reduce;

// Define the KYBER_NAMESPACE-prefixed symbols referenced by reduce.h declarations.
// The #define montgomery_reduce KYBER_NAMESPACE(montgomery_reduce) in reduce.h
// means callers call e.g. pqcrystals_kyber768_ref_montgomery_reduce(a).
// We provide that symbol here.

// Undefine the macro aliases before defining the real functions so the
// function names are not substituted by the preprocessor.
#undef montgomery_reduce
#undef barrett_reduce

int16_t KYBER_NAMESPACE(montgomery_reduce)(int32_t a) {
    return xinim::crypto::kyber::montgomery_reduce(a);
}

int16_t KYBER_NAMESPACE(barrett_reduce)(int16_t a) {
    return xinim::crypto::kyber::barrett_reduce(a);
}
