# Quaternion Library and Spinlock

This directory contains implementations for quaternion mathematics and a quaternion-based spinlock.

## Files

*   `quaternion.hpp`: Defines the `XINIM::Algebraic::Quaternion` class. This is a general-purpose mathematical quaternion class using `double` precision components. It supports various arithmetic operations, norm, inverse, conjugation, and normalization. It is designed with `constexpr` and `noexcept` for modern C++ usage and is aligned for SIMD.
*   `quaternion.cpp`: Contains the implementations for the non-trivial methods of the `XINIM::Algebraic::Quaternion` class.
*   `quaternion_spinlock.hpp`: Defines the `hyper::QuaternionSpinlock` class and its associated `hyper::Quaternion` (a minimal, `float`-based quaternion struct for the spinlock's internal logic) and `hyper::QuaternionLockGuard`. The spinlock uses `std::atomic_flag` for basic locking and the quaternion state for managing fairness or ticket rotation.

## Design Choices

### `XINIM::Algebraic::Quaternion` (General Purpose)

*   **Precision**: Uses `double` for components to maintain high precision for mathematical calculations.
*   **Alignment**: `alignas(32)` is used to potentially benefit from AVX SIMD instructions for 4 `double` components.
*   **`constexpr` and `noexcept`**: Used extensively to allow compile-time computations where possible and to indicate non-throwing behavior, suitable for kernel and performance-critical code.
*   **Error Handling**: Division by zero in `operator/=` and `inverse()` results in a zero quaternion rather than throwing exceptions, which is generally preferred in kernel contexts. Callers should check for zero quaternions if division by zero is a possibility.
*   **Namespace**: `XINIM::Algebraic`

### `hyper::Quaternion` and `hyper::QuaternionSpinlock` (Spinlock Specific)

*   **Precision**: The internal `hyper::Quaternion` struct uses `float` components. This is generally sufficient for the spinlock's orientation/ticket logic and can be faster.
*   **Alignment**: The `hyper::Quaternion` struct is `alignas(16)` for SSE compatibility (4 floats).
*   **Locking Mechanism**: `std::atomic_flag` provides the fundamental atomic test-and-set operation. The quaternion logic is layered on top for fairness.
*   **Cache-Line Awareness**: The spinlock attempts to align its `std::atomic_flag` and `Quaternion orientation` members to different cache lines using `alignas(std::hardware_destructive_interference_size)` where available (C++17) or manual padding as a fallback to reduce false sharing.
*   **Namespace**: `hyper` (kept from original implementation).

## Usage

### Mathematical Quaternions

Include `quaternion.hpp` and use the `XINIM::Algebraic::Quaternion` class for 3D rotations, orientation representation, or other algebraic calculations.

```cpp
#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp"

XINIM::Algebraic::Quaternion q1(1.0, 0.0, 0.0, 0.0); // Identity
XINIM::Algebraic::Quaternion q2(0.0, 1.0, 0.0, 0.0); // Pure i
auto product = q1 * q2;
```

### Quaternion Spinlock

Include `quaternion_spinlock.hpp`.

```cpp
#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion_spinlock.hpp"

hyper::QuaternionSpinlock my_lock;
// Ticket generation might depend on core_id or other fairness scheme
hyper::Quaternion ticket = hyper::Quaternion::id(); // Example ticket

{
    hyper::QuaternionLockGuard guard(my_lock, ticket);
    // Critical section
} // Lock automatically released
```

## Future Considerations

*   **SIMD Intrinsics**: For maximum performance, critical quaternion operations could be implemented using explicit SIMD intrinsics (SSE, AVX) for both `float` and `double` versions.
*   **Spinlock Fairness**: The exact fairness properties of the quaternion-based ticket system in the spinlock should be further analyzed and potentially enhanced for specific NUMA or core configurations.
*   **Type Unification/Conversion**: If the spinlock's `hyper::Quaternion` and the mathematical `XINIM::Algebraic::Quaternion` need to interoperate more closely, conversion functions or a unified type strategy might be considered, though their different precision and alignment serve distinct purposes currently.
