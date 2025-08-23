/**
 * @file quaternion_impl.hpp
 * @brief Implementation details for SIMD-optimized quaternion operations
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#pragma once

#include "quaternion.hpp"
#include "../detect.hpp"

namespace xinim::simd::math {

// Implementation of basic quaternion operations
template<typename T>
quaternion<T>& quaternion<T>::operator+=(const quaternion<T>& other) noexcept {
    if constexpr (std::is_same_v<T, float>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            // AVX implementation for single precision
            __m128 a = _mm_load_ps(reinterpret_cast<const float*>(this));
            __m128 b = _mm_load_ps(reinterpret_cast<const float*>(&other));
            __m128 result = _mm_add_ps(a, b);
            _mm_store_ps(reinterpret_cast<float*>(this), result);
            return *this;
        }
#endif
#if defined(__ARM_NEON)
        if (detect_capability(Capability::NEON)) {
            // NEON implementation for single precision
            float32x4_t a = vld1q_f32(reinterpret_cast<const float*>(this));
            float32x4_t b = vld1q_f32(reinterpret_cast<const float*>(&other));
            float32x4_t result = vaddq_f32(a, b);
            vst1q_f32(reinterpret_cast<float*>(this), result);
            return *this;
        }
#endif
    } else if constexpr (std::is_same_v<T, double>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            // AVX implementation for double precision
            __m256d a = _mm256_load_pd(reinterpret_cast<const double*>(this));
            __m256d b = _mm256_load_pd(reinterpret_cast<const double*>(&other));
            __m256d result = _mm256_add_pd(a, b);
            _mm256_store_pd(reinterpret_cast<double*>(this), result);
            return *this;
        }
#endif
    }
    
    // Scalar fallback
    w += other.w;
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

template<typename T>
quaternion<T>& quaternion<T>::operator-=(const quaternion<T>& other) noexcept {
    if constexpr (std::is_same_v<T, float>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            __m128 a = _mm_load_ps(reinterpret_cast<const float*>(this));
            __m128 b = _mm_load_ps(reinterpret_cast<const float*>(&other));
            __m128 result = _mm_sub_ps(a, b);
            _mm_store_ps(reinterpret_cast<float*>(this), result);
            return *this;
        }
#endif
#if defined(__ARM_NEON)
        if (detect_capability(Capability::NEON)) {
            float32x4_t a = vld1q_f32(reinterpret_cast<const float*>(this));
            float32x4_t b = vld1q_f32(reinterpret_cast<const float*>(&other));
            float32x4_t result = vsubq_f32(a, b);
            vst1q_f32(reinterpret_cast<float*>(this), result);
            return *this;
        }
#endif
    } else if constexpr (std::is_same_v<T, double>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            __m256d a = _mm256_load_pd(reinterpret_cast<const double*>(this));
            __m256d b = _mm256_load_pd(reinterpret_cast<const double*>(&other));
            __m256d result = _mm256_sub_pd(a, b);
            _mm256_store_pd(reinterpret_cast<double*>(this), result);
            return *this;
        }
#endif
    }
    
    // Scalar fallback
    w -= other.w;
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

template<typename T>
quaternion<T>& quaternion<T>::operator*=(T scalar) noexcept {
    if constexpr (std::is_same_v<T, float>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            __m128 a = _mm_load_ps(reinterpret_cast<const float*>(this));
            __m128 s = _mm_set1_ps(scalar);
            __m128 result = _mm_mul_ps(a, s);
            _mm_store_ps(reinterpret_cast<float*>(this), result);
            return *this;
        }
#endif
#if defined(__ARM_NEON)
        if (detect_capability(Capability::NEON)) {
            float32x4_t a = vld1q_f32(reinterpret_cast<const float*>(this));
            float32x4_t s = vdupq_n_f32(scalar);
            float32x4_t result = vmulq_f32(a, s);
            vst1q_f32(reinterpret_cast<float*>(this), result);
            return *this;
        }
#endif
    } else if constexpr (std::is_same_v<T, double>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            __m256d a = _mm256_load_pd(reinterpret_cast<const double*>(this));
            __m256d s = _mm256_set1_pd(scalar);
            __m256d result = _mm256_mul_pd(a, s);
            _mm256_store_pd(reinterpret_cast<double*>(this), result);
            return *this;
        }
#endif
    }
    
    // Scalar fallback
    w *= scalar;
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

template<typename T>
quaternion<T>& quaternion<T>::operator*=(const quaternion<T>& other) noexcept {
    *this = impl::get_optimal_multiply()(*this, other);
    return *this;
}

template<typename T>
quaternion<T> quaternion<T>::conjugate() const noexcept {
    if constexpr (std::is_same_v<T, float>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            __m128 a = _mm_load_ps(reinterpret_cast<const float*>(this));
            __m128 sign_mask = _mm_set_ps(-1.0f, -1.0f, -1.0f, 1.0f); // [w, -x, -y, -z]
            __m128 result = _mm_mul_ps(a, sign_mask);
            
            quaternion<T> conj;
            _mm_store_ps(reinterpret_cast<float*>(&conj), result);
            return conj;
        }
#endif
#if defined(__ARM_NEON)
        if (detect_capability(Capability::NEON)) {
            float32x4_t a = vld1q_f32(reinterpret_cast<const float*>(this));
            const float32x4_t sign_mask = {1.0f, -1.0f, -1.0f, -1.0f};
            float32x4_t result = vmulq_f32(a, sign_mask);
            
            quaternion<T> conj;
            vst1q_f32(reinterpret_cast<float*>(&conj), result);
            return conj;
        }
#endif
    }
    
    // Scalar fallback
    return quaternion<T>(w, -x, -y, -z);
}

template<typename T>
T quaternion<T>::norm_squared() const noexcept {
    if constexpr (std::is_same_v<T, float>) {
#if defined(__AVX__)
        if (detect_capability(Capability::AVX)) {
            __m128 a = _mm_load_ps(reinterpret_cast<const float*>(this));
            __m128 sq = _mm_mul_ps(a, a);
            __m128 sum = _mm_hadd_ps(sq, sq);
            sum = _mm_hadd_ps(sum, sum);
            return _mm_cvtss_f32(sum);
        }
#endif
#if defined(__ARM_NEON)
        if (detect_capability(Capability::NEON)) {
            float32x4_t a = vld1q_f32(reinterpret_cast<const float*>(this));
            float32x4_t sq = vmulq_f32(a, a);
            return vaddvq_f32(sq); // ARM64 only
        }
#endif
    }
    
    // Scalar fallback
    return w*w + x*x + y*y + z*z;
}

template<typename T>
T quaternion<T>::norm() const noexcept {
    return std::sqrt(norm_squared());
}

template<typename T>
quaternion<T> quaternion<T>::normalize() const noexcept {
    T n = norm();
    if (n > T{0}) {
        return *this * (T{1} / n);
    }
    return *this;
}

// Specialized implementations for different instruction sets
namespace impl {
    namespace scalar {
        template<typename T>
        quaternion<T> multiply(const quaternion<T>& a, const quaternion<T>& b) noexcept {
            return quaternion<T>(
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,  // w
                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,  // x
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,  // y
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w   // z
            );
        }
    }
    
#if defined(__AVX__)
    namespace avx {
        inline quaternionf multiply(const quaternionf& a, const quaternionf& b) noexcept {
            // Hand-tuned AVX quaternion multiplication
            __m128 a_vec = _mm_load_ps(reinterpret_cast<const float*>(&a));
            __m128 b_vec = _mm_load_ps(reinterpret_cast<const float*>(&b));
            
            // Broadcast each component of b
            __m128 b_w = _mm_shuffle_ps(b_vec, b_vec, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 b_x = _mm_shuffle_ps(b_vec, b_vec, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 b_y = _mm_shuffle_ps(b_vec, b_vec, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 b_z = _mm_shuffle_ps(b_vec, b_vec, _MM_SHUFFLE(3, 3, 3, 3));
            
            // Permute a for each multiplication
            __m128 a_wwww = _mm_shuffle_ps(a_vec, a_vec, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 a_xyzw = a_vec;
            __m128 a_yzxw = _mm_shuffle_ps(a_vec, a_vec, _MM_SHUFFLE(0, 1, 3, 2));
            __m128 a_zxyw = _mm_shuffle_ps(a_vec, a_vec, _MM_SHUFFLE(0, 2, 1, 3));
            
            // Compute products
            __m128 p0 = _mm_mul_ps(a_wwww, b_vec);
            __m128 p1 = _mm_mul_ps(a_xyzw, b_x);
            __m128 p2 = _mm_mul_ps(a_yzxw, b_y);
            __m128 p3 = _mm_mul_ps(a_zxyw, b_z);
            
            // Apply signs: [+, +, +, +], [-, +, +, -], [-, -, +, +], [-, +, -, +]
            __m128 sign1 = _mm_set_ps(-1.0f, 1.0f, 1.0f, -1.0f);
            __m128 sign2 = _mm_set_ps(1.0f, -1.0f, 1.0f, -1.0f);
            __m128 sign3 = _mm_set_ps(1.0f, 1.0f, -1.0f, -1.0f);
            
            p1 = _mm_mul_ps(p1, sign1);
            p2 = _mm_mul_ps(p2, sign2);
            p3 = _mm_mul_ps(p3, sign3);
            
            // Sum all products
            __m128 result = _mm_add_ps(p0, p1);
            result = _mm_add_ps(result, p2);
            result = _mm_add_ps(result, p3);
            
            quaternionf output;
            _mm_store_ps(reinterpret_cast<float*>(&output), result);
            return output;
        }
        
        inline quaterniond multiply(const quaterniond& a, const quaterniond& b) noexcept {
            // AVX double precision quaternion multiplication
            __m256d a_vec = _mm256_load_pd(reinterpret_cast<const double*>(&a));
            __m256d b_vec = _mm256_load_pd(reinterpret_cast<const double*>(&b));
            
            // This would require more complex shuffling for double precision
            // For now, fall back to scalar implementation
            return scalar::multiply(a, b);
        }
    }
#endif
    
#if defined(__ARM_NEON)
    namespace neon {
        inline quaternionf multiply(const quaternionf& a, const quaternionf& b) noexcept {
            float32x4_t a_vec = vld1q_f32(reinterpret_cast<const float*>(&a));
            float32x4_t b_vec = vld1q_f32(reinterpret_cast<const float*>(&b));
            
            // Extract components
            float32x4_t a_w = vdupq_laneq_f32(a_vec, 0);
            float32x4_t a_x = vdupq_laneq_f32(a_vec, 1);
            float32x4_t a_y = vdupq_laneq_f32(a_vec, 2);
            float32x4_t a_z = vdupq_laneq_f32(a_vec, 3);
            
            // Permute b for different products
            float32x4_t b_wxyz = b_vec;                                    // w, x, y, z
            float32x4_t b_xwzy = vextq_f32(b_vec, b_vec, 1);              // x, w, z, y (simplified)
            // ... more complex permutations needed
            
            // For now, use scalar fallback for correctness
            return scalar::multiply(a, b);
        }
    }
#endif
    
    // Runtime dispatch to get optimal implementation
    template<typename T>
    auto get_optimal_multiply() noexcept {
        if constexpr (std::is_same_v<T, float>) {
#if defined(__AVX__)
            if (detect_capability(Capability::AVX)) {
                return avx::multiply;
            }
#endif
#if defined(__ARM_NEON)
            if (detect_capability(Capability::NEON)) {
                return neon::multiply;
            }
#endif
        }
        return scalar::multiply<T>;
    }
}

// Batch operations implementation
namespace batch {
    template<typename T>
    void multiply(const quaternion<T>* a, const quaternion<T>* b, 
                 quaternion<T>* result, std::size_t count) noexcept {
        auto multiply_func = impl::get_optimal_multiply<T>();
        
        // Unroll for better performance
        std::size_t unroll_count = count & ~3UL; // Process 4 at a time
        std::size_t i = 0;
        
        for (; i < unroll_count; i += 4) {
            result[i]     = multiply_func(a[i],     b[i]);
            result[i + 1] = multiply_func(a[i + 1], b[i + 1]);
            result[i + 2] = multiply_func(a[i + 2], b[i + 2]);
            result[i + 3] = multiply_func(a[i + 3], b[i + 3]);
        }
        
        // Handle remaining elements
        for (; i < count; ++i) {
            result[i] = multiply_func(a[i], b[i]);
        }
    }
    
    template<typename T>
    void normalize(const quaternion<T>* input, quaternion<T>* output,
                  std::size_t count) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            output[i] = input[i].normalize();
        }
    }
}

// Explicit template instantiations
template class quaternion<float>;
template class quaternion<double>;

} // namespace xinim::simd::math
