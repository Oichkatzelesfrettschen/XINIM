/**
 * @file arm_sve.hpp  
 * @brief ARM SVE (Scalable Vector Extensions) SIMD abstractions for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Comprehensive ARM SVE/SVE2 support including:
 * - Scalable vectors (128-2048 bits)
 * - Predicated operations
 * - Gather/scatter operations
 * - Advanced mathematical functions
 * - Hand-tuned implementations for optimal performance
 */

#pragma once

#ifdef __ARM_FEATURE_SVE

#include "../core.hpp"
#include <arm_sve.h>
#include <cstdint>
#include <type_traits>

namespace xinim::simd::arch::arm::sve {

/**
 * @brief SVE capability detection
 */
class SVECapabilities {
public:
    static bool has_sve() noexcept;
    static bool has_sve2() noexcept;
    static bool has_sve_bf16() noexcept;
    static bool has_sve_i8mm() noexcept;
    static bool has_sve_f32mm() noexcept;
    static bool has_sve_f64mm() noexcept;
    static std::size_t get_vector_length() noexcept;
    static std::size_t get_max_vector_length() noexcept;
};

/**
 * @brief SVE scalable vector wrapper
 */
template<typename T>
class sve_vector {
private:
    using vector_type = std::conditional_t<
        std::is_same_v<T, float>, svfloat32_t,
        std::conditional_t<std::is_same_v<T, double>, svfloat64_t,
        std::conditional_t<std::is_same_v<T, std::int32_t>, svint32_t,
        std::conditional_t<std::is_same_v<T, std::uint32_t>, svuint32_t,
        std::conditional_t<std::is_same_v<T, std::int16_t>, svint16_t,
        std::conditional_t<std::is_same_v<T, std::uint16_t>, svuint16_t,
        std::conditional_t<std::is_same_v<T, std::int8_t>, svint8_t,
        std::conditional_t<std::is_same_v<T, std::uint8_t>, svuint8_t, void>>>>>>>>;
    
    using predicate_type = std::conditional_t<
        sizeof(T) == 1, svbool_t,
        std::conditional_t<sizeof(T) == 2, svbool_t,
        std::conditional_t<sizeof(T) == 4, svbool_t,
        std::conditional_t<sizeof(T) == 8, svbool_t, svbool_t>>>>;

    vector_type data;
    predicate_type pred;

public:
    using value_type = T;
    
    // Constructors
    sve_vector() : pred(svptrue<T>()) {}
    explicit sve_vector(vector_type v, predicate_type p = svptrue<T>()) 
        : data(v), pred(p) {}
    explicit sve_vector(T scalar) : pred(svptrue<T>()) {
        data = svdup_n<T>(scalar);
    }

    // Accessors
    vector_type native() const { return data; }
    predicate_type predicate() const { return pred; }
    std::size_t length() const { return svcntb() / sizeof(T); }
    
    // Arithmetic operations with predicates
    sve_vector operator+(const sve_vector& other) const;
    sve_vector operator-(const sve_vector& other) const;
    sve_vector operator*(const sve_vector& other) const;
    sve_vector operator/(const sve_vector& other) const;

    // Predicated operations
    sve_vector add_predicated(const sve_vector& other, predicate_type mask) const;
    sve_vector sub_predicated(const sve_vector& other, predicate_type mask) const;
    sve_vector mul_predicated(const sve_vector& other, predicate_type mask) const;

    // Load/Store with predicates
    void load(const T* ptr, predicate_type mask = svptrue<T>());
    void load_gather(const T* base, const sve_vector<std::int32_t>& indices,
                    predicate_type mask = svptrue<T>());
    void store(T* ptr, predicate_type mask = svptrue<T>()) const;
    void store_scatter(T* base, const sve_vector<std::int32_t>& indices,
                      predicate_type mask = svptrue<T>()) const;

    // Mathematical functions
    sve_vector sqrt() const;
    sve_vector rsqrt() const;
    sve_vector rcp() const;
    sve_vector abs() const;
    sve_vector min(const sve_vector& other) const;
    sve_vector max(const sve_vector& other) const;

    // Reduction operations
    T reduce_add() const;
    T reduce_min() const;
    T reduce_max() const;
    
    // Specialized SVE operations
    sve_vector fmadd(const sve_vector& mul, const sve_vector& add) const;
    sve_vector select(const sve_vector& false_val, predicate_type mask) const;
};

/**
 * @brief SVE quaternion operations (variable-width)
 */
namespace quaternion {
    struct sve_quaternion {
        svfloat32_t w, x, y, z;
        svbool_t pred;
        
        sve_quaternion() : pred(svptrue_b32()) {
            w = svdup_n_f32(0.0f);
            x = svdup_n_f32(0.0f);
            y = svdup_n_f32(0.0f);
            z = svdup_n_f32(0.0f);
        }
        
        sve_quaternion(svfloat32_t w_vec, svfloat32_t x_vec,
                      svfloat32_t y_vec, svfloat32_t z_vec,
                      svbool_t p = svptrue_b32())
            : w(w_vec), x(x_vec), y(y_vec), z(z_vec), pred(p) {}
        
        // Core operations optimized for variable vector lengths
        sve_quaternion multiply(const sve_quaternion& other) const;
        sve_quaternion conjugate() const;
        sve_quaternion inverse() const;
        svfloat32_t norm_squared() const;
        svfloat32_t norm() const;
        sve_quaternion normalize() const;
        
        // Batch operations across multiple quaternions
        void load_batch(const float* data, std::size_t stride);
        void store_batch(float* data, std::size_t stride) const;
        
        std::size_t vector_length() const { return svcntw(); }
    };
    
    // Optimized batch processing for arbitrary vector lengths
    void multiply_streaming(const float* a_data, const float* b_data,
                          float* result_data, std::size_t count);
    void normalize_streaming(const float* input_data, float* output_data,
                           std::size_t count);
    void conjugate_streaming(const float* input_data, float* output_data,
                           std::size_t count);
}

/**
 * @brief SVE double-precision quaternion operations
 */
namespace quaternion_f64 {
    struct sve_quaternion_f64 {
        svfloat64_t w, x, y, z;
        svbool_t pred;
        
        sve_quaternion_f64() : pred(svptrue_b64()) {
            w = svdup_n_f64(0.0);
            x = svdup_n_f64(0.0);
            y = svdup_n_f64(0.0);
            z = svdup_n_f64(0.0);
        }
        
        sve_quaternion_f64 multiply(const sve_quaternion_f64& other) const;
        sve_quaternion_f64 conjugate() const;
        svfloat64_t norm_squared() const;
        svfloat64_t norm() const;
        
        std::size_t vector_length() const { return svcntd(); }
    };
}

/**
 * @brief SVE octonion operations (scalable)
 */
namespace octonion {
    struct sve_octonion {
        svfloat32_t e[8]; // e0, e1, e2, e3, e4, e5, e6, e7
        svbool_t pred;
        
        sve_octonion() : pred(svptrue_b32()) {
            for (int i = 0; i < 8; ++i) {
                e[i] = svdup_n_f32(0.0f);
            }
        }
        
        sve_octonion multiply(const sve_octonion& other) const;
        sve_octonion conjugate() const;
        svfloat32_t norm_squared() const;
        svfloat32_t norm() const;
        
        // Efficient component access with gather/scatter
        void load_interleaved(const float* data, std::size_t stride);
        void store_interleaved(float* data, std::size_t stride) const;
        
        std::size_t vector_length() const { return svcntw(); }
    };
    
    // Streaming operations for large datasets
    void multiply_streaming(const float* a_data, const float* b_data,
                          float* result_data, std::size_t count);
    void conjugate_streaming(const float* input_data, float* output_data,
                           std::size_t count);
}

/**
 * @brief SVE sedenion operations (16 components)
 */
namespace sedenion {
    struct sve_sedenion {
        svfloat32_t e[16]; // e0 through e15
        svbool_t pred;
        
        sve_sedenion() : pred(svptrue_b32()) {
            for (int i = 0; i < 16; ++i) {
                e[i] = svdup_n_f32(0.0f);
            }
        }
        
        sve_sedenion multiply(const sve_sedenion& other) const;
        sve_sedenion conjugate() const;
        svfloat32_t norm_squared() const;
        
        // Advanced gather/scatter for complex memory layouts
        void load_aos(const float* data); // Array of structures
        void store_aos(float* data) const;
        void load_soa(const float* const* data); // Structure of arrays
        void store_soa(float* const* data) const;
        
        std::size_t vector_length() const { return svcntw(); }
    };
}

/**
 * @brief Advanced SVE memory operations
 */
namespace memory {
    // Gather/scatter operations
    template<typename T>
    void gather(const T* base, const sve_vector<std::int32_t>& indices,
               sve_vector<T>& result, svbool_t pred = svptrue<T>());
    
    template<typename T>
    void scatter(T* base, const sve_vector<std::int32_t>& indices,
                const sve_vector<T>& data, svbool_t pred = svptrue<T>());
    
    // Streaming operations with predicates
    void copy_predicated(const void* src, void* dst, std::size_t bytes,
                        svbool_t pred);
    void set_predicated(void* dst, std::uint8_t value, std::size_t bytes,
                       svbool_t pred);
    
    // Advanced prefetching
    void prefetch_gather(const void* base, const sve_vector<std::int32_t>& indices);
    void prefetch_streaming(const void* ptr, std::size_t bytes);
}

/**
 * @brief SVE-optimized string operations
 */
namespace string {
    // Variable-length string operations
    std::size_t strlen_sve(const char* str);
    int strcmp_sve(const char* a, const char* b);
    
    // Pattern matching with SVE predicates
    const char* strstr_sve(const char* haystack, const char* needle);
    const char* strchr_sve(const char* str, int ch);
    
    // Case conversion with predicates
    void toupper_sve(char* str, std::size_t len, svbool_t mask);
    void tolower_sve(char* str, std::size_t len, svbool_t mask);
    
    // Advanced string processing
    std::size_t count_chars_sve(const char* str, char ch);
    void replace_chars_sve(char* str, char from, char to, svbool_t mask);
}

/**
 * @brief Matrix operations using SVE
 */
namespace matrix {
    // Matrix-vector operations
    void matvec_f32(const float* matrix, const float* vector,
                   float* result, std::size_t rows, std::size_t cols);
    void matvec_f64(const double* matrix, const double* vector,
                   double* result, std::size_t rows, std::size_t cols);
    
    // Matrix-matrix operations
    void matmul_f32(const float* a, const float* b, float* c,
                   std::size_t m, std::size_t n, std::size_t k);
    
    // Specialized operations
    void transpose_f32(const float* input, float* output,
                      std::size_t rows, std::size_t cols);
    void vector_outer_product(const float* a, const float* b, float* result,
                             std::size_t size_a, std::size_t size_b);
}

/**
 * @brief SVE runtime utilities
 */
class SVERuntime {
public:
    static std::size_t get_current_vector_length();
    static std::size_t get_max_vector_length();
    static bool is_length_supported(std::size_t length);
    static void set_vector_length(std::size_t length);
    
    // Performance monitoring
    static void start_performance_counter();
    static std::uint64_t read_performance_counter();
    
    // Memory alignment helpers
    static std::size_t required_alignment();
    static bool is_aligned(const void* ptr);
    static void* aligned_alloc(std::size_t size);
    static void aligned_free(void* ptr);
};

} // namespace xinim::simd::arch::arm::sve

#endif // __ARM_FEATURE_SVE
