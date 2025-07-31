# Octonion Library

This directory contains the implementation for octonion mathematics within the XINIM OS kernel.

## Files

*   `octonion.hpp`: Defines the `XINIM::Algebraic::Octonion` class. This is a general-purpose mathematical octonion class using `double` precision components. It supports various arithmetic operations (addition, subtraction, scalar multiplication, Cayley-Dickson product), norm, inverse, conjugation, and normalization. It is designed with `constexpr` and `noexcept` for modern C++ usage and is aligned for SIMD (`alignas(64)` for 8 doubles).
*   `octonion.cpp`: Contains the implementations for the non-trivial methods of the `XINIM::Algebraic::Octonion` class.

## Design Choices

*   **Precision**: Uses `double` for components to maintain high precision for mathematical calculations.
*   **Alignment**: `alignas(64)` is used to potentially benefit from AVX-512 SIMD instructions for 8 `double` components.
*   **`constexpr` and `noexcept`**: Used extensively to allow compile-time computations where possible and to indicate non-throwing behavior, suitable for kernel and performance-critical code.
*   **Error Handling**: Division by zero in `operator/=` and `inverse()` results in a zero octonion rather than throwing exceptions, which is generally preferred in kernel contexts. Callers should check for zero octonions if division by zero is a possibility.
*   **Cayley-Dickson Construction**: Octonion multiplication and construction from two quaternions follow the Cayley-Dickson construction rules. The `XINIM::Algebraic::Quaternion` type from the quaternion library is used for this.
*   **Namespace**: `XINIM::Algebraic`

## Usage

Include `octonion.hpp` and use the `XINIM::Algebraic::Octonion` class.

```cpp
#include "KERNEL/IPC/ALGEBRAIC/octonion/octonion.hpp"
#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp" // If constructing from quaternions

// Using scalar constructor
XINIM::Algebraic::Octonion o1(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);

// Using identity or zero
XINIM::Algebraic::Octonion id = XINIM::Algebraic::Octonion::identity();
XINIM::Algebraic::Octonion zero = XINIM::Algebraic::Octonion::zero();

// Constructing from two quaternions
XINIM::Algebraic::Quaternion q_a(1,2,3,4);
XINIM::Algebraic::Quaternion q_b(5,6,7,8);
XINIM::Algebraic::Octonion o_from_q(q_a, q_b);

// Operations
XINIM::Algebraic::Octonion o2 = o1.conjugate();
XINIM::Algebraic::Octonion product = o1 * o_from_q; // Non-associative
double n = o1.norm();
```

## Relation to `lattice::Octonion`

The codebase also contains a `lattice::Octonion` (found in `kernel/octonion.hpp`). This is a specialized octonion type using `std::array<std::uint32_t, 8>` for its components and is designed for use as capability tokens, featuring `from_bytes` and `to_bytes` methods.

The `XINIM::Algebraic::Octonion` in this directory is intended as the general-purpose, floating-point based algebraic type for broader mathematical use within the kernel, including potentially as a basis for other higher-order algebras or for physics/geometry calculations if needed. The `lattice::Octonion` remains distinct for its specific token representation needs.

## Future Considerations

*   **SIMD Intrinsics**: For maximum performance, critical octonion operations could be implemented using explicit SIMD intrinsics (SSE, AVX, AVX-512).
*   **Constant-Time Operations**: If any octonion operations are identified as needing to be strictly constant-time for security reasons (e.g., if used in cryptographic primitives beyond simple capability tokens), those specific operations would require careful review and potential reimplementation. This is generally not a concern for a mathematical library using floating-point types but is noted.
