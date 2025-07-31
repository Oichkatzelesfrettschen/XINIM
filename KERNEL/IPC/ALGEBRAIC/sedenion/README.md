# Sedenion Library

This directory contains the implementation for sedenion mathematics within the XINIM OS kernel.

## Files

*   `sedenion.hpp`: Defines the `XINIM::Algebraic::Sedenion` class. This is a general-purpose mathematical sedenion class using `double` precision components. It supports basic arithmetic operations, norm, conjugate, and operations related to their non-division algebra nature (e.g., checking for potential zero-divisors). It is aligned for SIMD (`alignas(128)` for 16 doubles).
    *   Includes placeholder structures and functions for conceptual security applications using sedenions, such as `quantum_signature_t`, `compute_hash_sedenion`, and `compute_complementary_sedenion`. These are for API design exploration and require significant cryptographic research to be made viable.
*   `sedenion.cpp`: Contains the implementations for the non-trivial methods of the `XINIM::Algebraic::Sedenion` class, including the placeholder security functions.

## Design Choices

*   **Precision**: Uses `double` for components to maintain high precision for mathematical calculations.
*   **Alignment**: `alignas(128)` is used to potentially benefit from AVX-512 SIMD instructions for 16 `double` components.
*   **`constexpr` and `noexcept`**: Used extensively for compile-time computations and indicating non-throwing behavior, suitable for kernel code.
*   **Zero Divisors**: Sedenions are not a division algebra. The `inverse()` method returns a zero sedenion if the norm-squared is near zero (indicating a potential zero divisor or the zero sedenion itself). The method `is_potential_zero_divisor()` helps identify such cases.
*   **Error Handling**: Operations like division by a near-zero scalar result in a zero sedenion rather than throwing exceptions.
*   **Cayley-Dickson Construction**: Sedenion multiplication and construction from two octonions follow the Cayley-Dickson construction, using the `XINIM::Algebraic::Octonion` type.
*   **Namespace**: `XINIM::Algebraic`.
*   **Security Placeholders**: The security-related functions (`compute_hash_sedenion`, `compute_complementary_sedenion`) and `quantum_signature_t` are highly conceptual and serve as API placeholders for future research into sedenion-based cryptographic schemes. Their current implementations are trivial and not cryptographically secure.

## Usage

Include `sedenion.hpp` and use the `XINIM::Algebraic::Sedenion` class.

```cpp
#include "KERNEL/IPC/ALGEBRAIC/sedenion/sedenion.hpp"
#include "KERNEL/IPC/ALGEBRAIC/octonion/octonion.hpp" // If constructing from octonions

// Using scalar constructor or identity/zero
XINIM::Algebraic::Sedenion s1(1.0, ..., 0.0); // 16 components
XINIM::Algebraic::Sedenion id = XINIM::Algebraic::Sedenion::identity();

// Constructing from two octonions
XINIM::Algebraic::Octonion o_a(...);
XINIM::Algebraic::Octonion o_b(...);
XINIM::Algebraic::Sedenion s_from_o(o_a, o_b);

// Operations
XINIM::Algebraic::Sedenion s_conj = s1.conjugate();
XINIM::Algebraic::Sedenion product = s1 * s_from_o; // Non-associative, has zero divisors
double n_sq = s1.norm_sq();

// Conceptual security API
unsigned char my_data[] = "hello";
XINIM::Algebraic::Sedenion hashed_s = XINIM::Algebraic::Sedenion::compute_hash_sedenion(my_data, sizeof(my_data));
XINIM::Algebraic::Sedenion complement_s = hashed_s.compute_complementary_sedenion();

// Note: The cryptographic functions are placeholders and not secure.
```

## Relation to `kernel/sedenion.hpp` (Float-based, Experimental)

The codebase previously contained a `kernel/sedenion.hpp` with a `float`-based sedenion implementation that included some experimental cryptographic functions like `zlock_encrypt`. That file has been superseded by this more general `XINIM::Algebraic::Sedenion` library, which is `double`-based (for mathematical fidelity) and incorporates the *signatures* of the security concepts from the project requirements as placeholders. The actual cryptographic mechanisms from the older file would need to be re-evaluated and potentially adapted if they are to be pursued. This current library provides the algebraic foundation.

## Future Considerations

*   **SIMD Intrinsics**: Implementations using explicit SIMD intrinsics for performance.
*   **Zero Divisor Research**: The properties of sedenion zero divisors are complex. The `compute_complementary_sedenion` is a placeholder for what would be a non-trivial function to find a `b` such that `s*b = 0` for a given `s`. This is an area for further mathematical and algorithmic research if specific zero-divisor properties are to be exploited.
*   **Cryptographic Primitives**: Developing secure cryptographic primitives using sedenions is a significant research effort. The current placeholders are merely for API design exploration.
*   **Performance of 16-double operations**: For frequent, performance-critical operations, careful optimization or choosing `float` components (sacrificing precision) might be necessary.
