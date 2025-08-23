/**
 * @file arm_neon.hpp
 * @brief ARM NEON and VFP SIMD instruction set abstractions for XINIM
 * @author XINIM Project  
 * @version 1.0
 * @date 2025
 * 
 * Comprehensive ARM NEON SIMD support including:
 * - VFPv3/VFPv4 floating-point units
 * - NEON 64-bit and 128-bit SIMD
 * - Crypto extensions (AES, SHA, PMULL)
 * - Half-precision (FP16) support
 * - Hand-tuned unrolled loops for optimal performance
 */

#pragma once

#ifdef __aarch64__

#include "../core.hpp"
#include <arm_neon.h>
#include <cstdint>
#include <type_traits>

namespace xinim::simd::arch::arm {

/**
 * @brief ARM NEON capability detection
 */
class NEONCapabilities {
public:
    static bool has_vfp() noexcept;
    static bool has_vfpv3() noexcept;
    static bool has_vfpv4() noexcept;
    static bool has_neon() noexcept;
    static bool has_neon_fp16() noexcept;
    static bool has_crypto() noexcept;
    static bool has_crc32() noexcept;
    static bool has_pmull() noexcept;
    static bool has_sha1() noexcept;
    static bool has_sha2() noexcept;
    static bool has_aes() noexcept;
};

/**
 * @brief NEON 128-bit vector wrapper
 */
template<typename T>
class neon_vector {
private:
    using vector_type = std::conditional_t<
        std::is_same_v<T, float>, float32x4_t,
        std::conditional_t<std::is_same_v<T, double>, float64x2_t,
        std::conditional_t<std::is_same_v<T, std::int32_t>, int32x4_t,
        std::conditional_t<std::is_same_v<T, std::uint32_t>, uint32x4_t,
        std::conditional_t<std::is_same_v<T, std::int16_t>, int16x8_t,
        std::conditional_t<std::is_same_v<T, std::uint16_t>, uint16x8_t,
        std::conditional_t<std::is_same_v<T, std::int8_t>, int8x16_t,
        std::conditional_t<std::is_same_v<T, std::uint8_t>, uint8x16_t, void>>>>>>>>;
    
    vector_type data;

public:
    using value_type = T;
    static constexpr std::size_t width = 128;
    static constexpr std::size_t elements = width / (sizeof(T) * 8);

    // Constructors
    neon_vector() = default;
    explicit neon_vector(vector_type v) : data(v) {}
    explicit neon_vector(T scalar);
    neon_vector(const T* ptr);

    // Accessors
    vector_type native() const { return data; }
    T extract(std::size_t index) const;
    void insert(std::size_t index, T value);

    // Arithmetic operations
    neon_vector operator+(const neon_vector& other) const;
    neon_vector operator-(const neon_vector& other) const;
    neon_vector operator*(const neon_vector& other) const;
    neon_vector operator/(const neon_vector& other) const;

    // Compound assignment
    neon_vector& operator+=(const neon_vector& other);
    neon_vector& operator-=(const neon_vector& other);
    neon_vector& operator*=(const neon_vector& other);
    neon_vector& operator/=(const neon_vector& other);

    // Load/Store
    void load(const T* ptr);
    void load_unaligned(const T* ptr);
    void store(T* ptr) const;
    void store_unaligned(T* ptr) const;

    // Mathematical functions
    neon_vector sqrt() const;
    neon_vector rsqrt() const;
    neon_vector rcp() const;
    neon_vector abs() const;
    neon_vector min(const neon_vector& other) const;
    neon_vector max(const neon_vector& other) const;

    // Specialized operations
    neon_vector fmadd(const neon_vector& mul, const neon_vector& add) const;
    neon_vector fmsub(const neon_vector& mul, const neon_vector& sub) const;
};

/**
 * @brief Optimized float32x4 quaternion operations
 */
namespace quaternion {
    struct alignas(16) neon_quaternion {
        float32x4_t data; // [w, x, y, z]
        
        neon_quaternion() = default;
        neon_quaternion(float w, float x, float y, float z);
        neon_quaternion(float32x4_t v) : data(v) {}
        
        // Core operations with hand-tuned assembly
        neon_quaternion multiply(const neon_quaternion& other) const;
        neon_quaternion conjugate() const;
        neon_quaternion inverse() const;
        float norm_squared() const;
        float norm() const;
        neon_quaternion normalize() const;
        
        // Rotation operations
        neon_quaternion slerp(const neon_quaternion& target, float t) const;
        float32x4_t rotate_vector(float32x4_t vec) const;
    };
    
    // Unrolled batch operations
    void multiply_batch_4(const neon_quaternion* a, const neon_quaternion* b, 
                         neon_quaternion* result);
    void multiply_batch_8(const neon_quaternion* a, const neon_quaternion* b, 
                         neon_quaternion* result);
    void normalize_batch_4(const neon_quaternion* input, neon_quaternion* output);
    void conjugate_batch_8(const neon_quaternion* input, neon_quaternion* output);
}

/**
 * @brief Optimized double precision quaternion operations
 */
namespace quaternion_f64 {
    struct alignas(32) neon_quaternion_f64 {
        float64x2_t data_lo; // [w, x]
        float64x2_t data_hi; // [y, z]
        
        neon_quaternion_f64() = default;
        neon_quaternion_f64(double w, double x, double y, double z);
        
        neon_quaternion_f64 multiply(const neon_quaternion_f64& other) const;
        neon_quaternion_f64 conjugate() const;
        double norm_squared() const;
        double norm() const;
    };
}

/**
 * @brief Optimized octonion operations using NEON
 */
namespace octonion {
    struct alignas(32) neon_octonion {
        float32x4_t data_lo; // [e0, e1, e2, e3]
        float32x4_t data_hi; // [e4, e5, e6, e7]
        
        neon_octonion() = default;
        neon_octonion(float e0, float e1, float e2, float e3,
                     float e4, float e5, float e6, float e7);
        
        neon_octonion multiply(const neon_octonion& other) const;
        neon_octonion conjugate() const;
        float norm_squared() const;
        float norm() const;
        
        // Cayley-Dickson construction helpers
        quaternion::neon_quaternion get_quaternion_a() const;
        quaternion::neon_quaternion get_quaternion_b() const;
        void set_from_quaternions(const quaternion::neon_quaternion& a,
                                const quaternion::neon_quaternion& b);
    };
    
    // Batch operations for performance
    void multiply_batch_2(const neon_octonion* a, const neon_octonion* b,
                         neon_octonion* result);
    void conjugate_batch_4(const neon_octonion* input, neon_octonion* output);
}

/**
 * @brief Memory operations optimized for NEON
 */
namespace memory {
    // Optimized memory operations
    void copy_aligned_128(const void* src, void* dst, std::size_t bytes);
    void copy_unaligned_128(const void* src, void* dst, std::size_t bytes);
    void set_aligned_128(void* dst, std::uint8_t value, std::size_t bytes);
    void zero_aligned_128(void* dst, std::size_t bytes);
    
    // Compare operations
    int compare_aligned_128(const void* a, const void* b, std::size_t bytes);
    bool equal_aligned_128(const void* a, const void* b, std::size_t bytes);
    
    // Streaming operations for large data
    void streaming_copy_128(const void* src, void* dst, std::size_t bytes);
    void streaming_set_128(void* dst, std::uint8_t value, std::size_t bytes);
}

/**
 * @brief String operations optimized for NEON
 */
namespace string {
    // Optimized string operations
    std::size_t strlen_neon(const char* str);
    int strcmp_neon(const char* a, const char* b);
    int strncmp_neon(const char* a, const char* b, std::size_t n);
    char* strcpy_neon(char* dst, const char* src);
    char* strncpy_neon(char* dst, const char* src, std::size_t n);
    
    // Search operations  
    const char* strchr_neon(const char* str, int ch);
    const char* strstr_neon(const char* haystack, const char* needle);
    
    // Case conversion
    void toupper_neon(char* str, std::size_t len);
    void tolower_neon(char* str, std::size_t len);
}

/**
 * @brief Cryptographic operations using NEON crypto extensions
 */
namespace crypto {
    // AES operations (requires crypto extensions)
    void aes_encrypt_128(const std::uint8_t* plaintext, 
                        const std::uint8_t* key,
                        std::uint8_t* ciphertext);
    void aes_decrypt_128(const std::uint8_t* ciphertext,
                        const std::uint8_t* key, 
                        std::uint8_t* plaintext);
    
    // SHA operations
    void sha1_update_neon(std::uint32_t* state, const std::uint8_t* data);
    void sha256_update_neon(std::uint32_t* state, const std::uint8_t* data);
    
    // PMULL operations for GCM and CRC
    std::uint64_t pmull_64(std::uint64_t a, std::uint64_t b);
    std::uint32_t crc32_neon(std::uint32_t crc, const std::uint8_t* data, std::size_t len);
}

/**
 * @brief Runtime feature detection implementation
 */
class RuntimeDetection {
public:
    static Capability detect_capabilities();
    static bool is_supported(Capability cap);
    static const char* capability_name(Capability cap);
};

} // namespace xinim::simd::arch::arm

#endif // __aarch64__
