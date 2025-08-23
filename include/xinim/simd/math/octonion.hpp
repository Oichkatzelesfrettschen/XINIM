/**
 * @file octonion.hpp
 * @brief Unified SIMD-optimized octonion operations for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * High-performance octonion library supporting:
 * - All major SIMD instruction sets (x86-64, ARM NEON/SVE)
 * - Non-associative algebra operations
 * - Cayley-Dickson construction
 * - Fano plane multiplication tables
 * - Batch processing for multiple octonions
 * - Advanced mathematical operations
 */

#pragma once

#include "../core.hpp"
#include "../detect.hpp"
#include "quaternion.hpp"
#include <cmath>
#include <array>
#include <concepts>
#include <type_traits>

namespace xinim::simd::math {

/**
 * @brief Base octonion template with SIMD optimization
 */
template<typename T>
    requires std::floating_point<T>
class alignas(std::max(64UL, sizeof(T) * 8)) octonion {
public:
    using value_type = T;
    using quaternion_type = quaternion<T>;
    static constexpr std::size_t alignment = std::max(64UL, sizeof(T) * 8);
    static constexpr std::size_t num_components = 8;
    
    // Component storage (e0, e1, e2, e3, e4, e5, e6, e7)
    union {
        std::array<T, 8> components;
        struct { T e0, e1, e2, e3, e4, e5, e6, e7; };
        struct { quaternion_type a, b; }; // Cayley-Dickson representation
        
        // SIMD-specific storage
#ifdef __AVX512F__
        __m512d avx512_data;    // For double precision
        __m512  avx512_data_f;  // For single precision
#endif
#ifdef __AVX__
        struct { __m256d lo, hi; } avx_data;   // For double precision
        struct { __m256  lo, hi; } avx_data_f; // For single precision  
#endif
#ifdef __ARM_NEON
        struct { float32x4_t lo, hi; } neon_data;     // For single precision ARM
        struct { float64x2_t q0, q1, q2, q3; } neon_f64; // For double precision ARM
#endif
    };

    // Constructors
    constexpr octonion() noexcept : components{} {}
    
    constexpr octonion(T e0_val, T e1_val, T e2_val, T e3_val,
                      T e4_val, T e5_val, T e6_val, T e7_val) noexcept
        : components{e0_val, e1_val, e2_val, e3_val, e4_val, e5_val, e6_val, e7_val} {}
    
    constexpr explicit octonion(const std::array<T, 8>& comp) noexcept
        : components(comp) {}
    
    // Cayley-Dickson construction from two quaternions
    octonion(const quaternion_type& q_a, const quaternion_type& q_b) noexcept
        : a(q_a), b(q_b) {}
    
    // Copy and move constructors
    octonion(const octonion&) = default;
    octonion& operator=(const octonion&) = default;
    octonion(octonion&&) = default;
    octonion& operator=(octonion&&) = default;

    // Component access
    constexpr T& operator[](std::size_t index) noexcept {
        return components[index];
    }
    
    constexpr const T& operator[](std::size_t index) const noexcept {
        return components[index];
    }

    // Quaternion access for Cayley-Dickson operations
    quaternion_type& get_a() noexcept { return a; }
    const quaternion_type& get_a() const noexcept { return a; }
    quaternion_type& get_b() noexcept { return b; }
    const quaternion_type& get_b() const noexcept { return b; }

    // Basic arithmetic operators (optimized with SIMD when available)
    octonion& operator+=(const octonion& other) noexcept;
    octonion& operator-=(const octonion& other) noexcept;
    octonion& operator*=(const octonion& other) noexcept; // Non-associative product
    octonion& operator*=(T scalar) noexcept;
    octonion& operator/=(T scalar) noexcept;

    // Non-member operator friends
    friend octonion operator+(octonion lhs, const octonion& rhs) noexcept {
        return lhs += rhs;
    }
    
    friend octonion operator-(octonion lhs, const octonion& rhs) noexcept {
        return lhs -= rhs;
    }
    
    friend octonion operator*(octonion lhs, const octonion& rhs) noexcept {
        return lhs *= rhs;
    }
    
    friend octonion operator*(octonion o, T scalar) noexcept {
        return o *= scalar;
    }
    
    friend octonion operator*(T scalar, octonion o) noexcept {
        return o *= scalar;
    }

    // Core octonion operations
    [[nodiscard]] octonion conjugate() const noexcept;
    [[nodiscard]] octonion inverse() const noexcept;
    [[nodiscard]] T norm_squared() const noexcept;
    [[nodiscard]] T norm() const noexcept;
    [[nodiscard]] octonion normalize() const noexcept;
    [[nodiscard]] bool is_unit(T tolerance = T{1e-6}) const noexcept;
    [[nodiscard]] bool is_zero(T tolerance = T{1e-12}) const noexcept;

    // Non-associative properties
    [[nodiscard]] bool is_associative_with(const octonion& other) const noexcept;
    [[nodiscard]] T associativity_measure(const octonion& b, const octonion& c) const noexcept;

    // Fano plane operations (specific octonion algebra structure)
    [[nodiscard]] octonion fano_multiply(const octonion& other) const noexcept;
    [[nodiscard]] std::array<int, 7> get_fano_coordinates() const noexcept;
    static octonion from_fano_coordinates(const std::array<int, 7>& coords) noexcept;

    // Special octonion operations
    [[nodiscard]] octonion commutator(const octonion& other) const noexcept; // [a,b] = ab - ba
    [[nodiscard]] octonion associator(const octonion& b, const octonion& c) const noexcept; // [a,b,c]
    [[nodiscard]] octonion moufang_left(const octonion& b, const octonion& c) const noexcept;
    [[nodiscard]] octonion moufang_right(const octonion& b, const octonion& c) const noexcept;

    // G2 group operations (octonion automorphisms)
    [[nodiscard]] octonion g2_transform(const std::array<std::array<T, 8>, 8>& matrix) const noexcept;
    static std::array<std::array<T, 8>, 8> g2_generator(int index) noexcept; // index 0-13

    // Triality and exceptional structures
    [[nodiscard]] std::array<T, 8> triality_left(const std::array<T, 8>& vector) const noexcept;
    [[nodiscard]] std::array<T, 8> triality_right(const std::array<T, 8>& vector) const noexcept;

    // Comparison operators
    [[nodiscard]] bool operator==(const octonion& other) const noexcept;
    [[nodiscard]] bool operator!=(const octonion& other) const noexcept {
        return !(*this == other);
    }
    
    [[nodiscard]] bool approximately_equal(const octonion& other, 
                                         T tolerance = T{1e-6}) const noexcept;

    // Special octonions
    static constexpr octonion identity() noexcept {
        return octonion(T{1}, T{0}, T{0}, T{0}, T{0}, T{0}, T{0}, T{0});
    }
    
    static constexpr octonion zero() noexcept {
        return octonion(T{0}, T{0}, T{0}, T{0}, T{0}, T{0}, T{0}, T{0});
    }
    
    // Standard basis octonions
    static constexpr octonion e(std::size_t index) noexcept {
        octonion result;
        if (index < 8) {
            result.components[index] = T{1};
        }
        return result;
    }
};

// Type aliases for common precision levels
using octonionf = octonion<float>;
using octoniond = octonion<double>;

/**
 * @brief Fano plane multiplication table for octonions
 */
template<typename T>
class fano_multiplication_table {
private:
    static constexpr std::array<std::array<int, 8>, 8> multiplication_signs = {{
        {{ 1,  0,  0,  0,  0,  0,  0,  0}},  // e0 (scalar)
        {{ 0,  1,  0,  0,  0,  0,  0,  0}},  // e1
        {{ 0,  0,  1,  0,  0,  0,  0,  0}},  // e2
        {{ 0,  0,  0,  1,  0,  0,  0,  0}},  // e3
        {{ 0,  0,  0,  0,  1,  0,  0,  0}},  // e4
        {{ 0,  0,  0,  0,  0,  1,  0,  0}},  // e5
        {{ 0,  0,  0,  0,  0,  0,  1,  0}},  // e6
        {{ 0,  0,  0,  0,  0,  0,  0,  1}}   // e7
    }};
    
    static constexpr std::array<std::array<int, 8>, 8> fano_table = {{
        {{ 0,  1,  2,  3,  4,  5,  6,  7}},
        {{ 1, -1,  3, -2,  5, -4, -7,  6}},
        {{ 2, -3, -1,  1,  6,  7, -4, -5}},
        {{ 3,  2, -1, -1,  7, -6,  5, -4}},
        {{ 4, -5, -6, -7, -1,  1,  2,  3}},
        {{ 5,  4, -7,  6, -1, -1, -3,  2}},
        {{ 6,  7,  4, -5, -2,  3, -1, -1}},
        {{ 7, -6,  5,  4, -3, -2,  1, -1}}
    }};

public:
    static constexpr int multiply_basis(std::size_t i, std::size_t j) noexcept {
        return (i < 8 && j < 8) ? fano_table[i][j] : 0;
    }
    
    static constexpr int sign(std::size_t i, std::size_t j) noexcept {
        int result = multiply_basis(i, j);
        return (result < 0) ? -1 : ((result > 0) ? 1 : 0);
    }
    
    static constexpr std::size_t index(std::size_t i, std::size_t j) noexcept {
        int result = multiply_basis(i, j);
        return static_cast<std::size_t>(result < 0 ? -result : result);
    }
};

/**
 * @brief Batch octonion operations for SIMD optimization
 */
namespace batch {
    // Batch multiply operations (non-associative)
    template<typename T>
    void multiply(const octonion<T>* a, const octonion<T>* b, 
                 octonion<T>* result, std::size_t count) noexcept;
    
    template<typename T>
    void multiply_scalar(const octonion<T>* input, T scalar,
                        octonion<T>* result, std::size_t count) noexcept;
    
    // Batch normalization
    template<typename T>
    void normalize(const octonion<T>* input, octonion<T>* output,
                  std::size_t count) noexcept;
    
    // Batch conjugation
    template<typename T>
    void conjugate(const octonion<T>* input, octonion<T>* output,
                  std::size_t count) noexcept;
    
    // Batch inverse
    template<typename T>
    void inverse(const octonion<T>* input, octonion<T>* output,
                std::size_t count) noexcept;
    
    // Batch Fano plane operations
    template<typename T>
    void fano_multiply(const octonion<T>* a, const octonion<T>* b,
                      octonion<T>* result, std::size_t count) noexcept;
    
    // Batch G2 transformations
    template<typename T>
    void g2_transform(const octonion<T>* input, 
                     const std::array<std::array<T, 8>, 8>& matrix,
                     octonion<T>* output, std::size_t count) noexcept;
}

/**
 * @brief Specialized octonion implementations for different SIMD instruction sets
 */
namespace impl {
    // Scalar fallback implementations
    namespace scalar {
        template<typename T>
        octonion<T> multiply(const octonion<T>& a, const octonion<T>& b) noexcept;
        
        template<typename T>
        octonion<T> fano_multiply(const octonion<T>& a, const octonion<T>& b) noexcept;
        
        template<typename T>
        octonion<T> normalize(const octonion<T>& o) noexcept;
    }
    
    // SSE implementations (x86-64)
    namespace sse {
        octonionf multiply(const octonionf& a, const octonionf& b) noexcept;
        octonionf normalize(const octonionf& o) noexcept;
        void batch_multiply(const octonionf* a, const octonionf* b,
                           octonionf* result, std::size_t count) noexcept;
    }
    
    // AVX implementations (x86-64)
    namespace avx {
        octonionf multiply(const octonionf& a, const octonionf& b) noexcept;
        octoniond multiply(const octoniond& a, const octoniond& b) noexcept;
        octonionf normalize(const octonionf& o) noexcept;
        octoniond normalize(const octoniond& o) noexcept;
        
        void batch_multiply_f32(const octonionf* a, const octonionf* b,
                               octonionf* result, std::size_t count) noexcept;
        void batch_multiply_f64(const octoniond* a, const octoniond* b,
                               octoniond* result, std::size_t count) noexcept;
    }
    
    // AVX-512 implementations (x86-64)  
    namespace avx512 {
        octonionf multiply(const octonionf& a, const octonionf& b) noexcept;
        octoniond multiply(const octoniond& a, const octoniond& b) noexcept;
        
        void batch_multiply_16(const octonionf* a, const octonionf* b,
                              octonionf* result) noexcept; // Process 16 at once
        void batch_multiply_8(const octoniond* a, const octoniond* b,
                             octoniond* result) noexcept; // Process 8 at once
        
        void fano_multiply_batch(const octonionf* a, const octonionf* b,
                                octonionf* result, std::size_t count) noexcept;
    }
    
    // ARM NEON implementations
    namespace neon {
        octonionf multiply(const octonionf& a, const octonionf& b) noexcept;
        octonionf normalize(const octonionf& o) noexcept;
        void batch_multiply(const octonionf* a, const octonionf* b,
                           octonionf* result, std::size_t count) noexcept;
    }
    
    // ARM SVE implementations
    namespace sve {
        void multiply_streaming(const float* a_data, const float* b_data,
                               float* result_data, std::size_t count) noexcept;
        void normalize_streaming(const float* input_data, float* output_data,
                                std::size_t count) noexcept;
        void fano_multiply_streaming(const float* a_data, const float* b_data,
                                    float* result_data, std::size_t count) noexcept;
    }
}

/**
 * @brief E8 lattice operations using octonions
 */
namespace e8_lattice {
    template<typename T>
    struct e8_point {
        std::array<T, 8> coordinates;
        
        [[nodiscard]] T norm_squared() const noexcept;
        [[nodiscard]] T inner_product(const e8_point& other) const noexcept;
        [[nodiscard]] e8_point project_to_lattice() const noexcept;
    };
    
    template<typename T>
    e8_point<T> octonion_to_e8(const octonion<T>& o) noexcept;
    
    template<typename T>
    octonion<T> e8_to_octonion(const e8_point<T>& point) noexcept;
    
    // Lattice operations
    template<typename T>
    std::vector<e8_point<T>> nearest_neighbors(const e8_point<T>& point) noexcept;
    
    template<typename T>
    T kissing_number() noexcept; // Should return 240 for optimal packing
}

} // namespace xinim::simd::math

// Include implementation details
#include "octonion_impl.hpp"
