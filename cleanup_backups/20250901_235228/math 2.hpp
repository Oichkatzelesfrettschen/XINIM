/**
 * @file math.hpp
 * @brief Main SIMD mathematics library header for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Unified header for all SIMD-optimized mathematical operations including:
 * - Quaternions, octonions, sedenions
 * - Complex numbers and hypercomplex algebras
 * - Vector and matrix operations
 * - Runtime dispatch and compile-time optimization
 */

#pragma once

// Core SIMD infrastructure
#include "../core.hpp"
#include "../detect.hpp"

// Mathematical types
#include "quaternion.hpp"
#include "octonion.hpp"
#include "sedenion.hpp"

namespace xinim::simd::math {

/**
 * @brief Runtime mathematical library initialization
 */
class MathLibrary {
public:
    // Initialize the math library with optimal implementations
    static void initialize() noexcept;
    
    // Get current capabilities
    static Capability get_capabilities() noexcept;
    
    // Performance information
    static const char* get_implementation_name() noexcept;
    static bool is_optimized() noexcept;
    
    // Benchmark utilities
    static void benchmark_quaternions() noexcept;
    static void benchmark_octonions() noexcept;
    static void benchmark_sedenions() noexcept;
    
    // Memory alignment helpers
    static std::size_t get_required_alignment() noexcept;
    static void* aligned_alloc(std::size_t size) noexcept;
    static void aligned_free(void* ptr) noexcept;
    
private:
    static inline bool initialized_ = false;
    static inline Capability capabilities_ = static_cast<Capability>(0);
    static inline const char* implementation_name_ = "uninitialized";
};

/**
 * @brief Unified dispatch interface for all mathematical operations
 */
namespace dispatch {
    // Quaternion operation dispatchers
    template<typename T>
    using quaternion_multiply_func = quaternion<T>(*)(const quaternion<T>&, const quaternion<T>&);
    
    template<typename T>
    using quaternion_normalize_func = quaternion<T>(*)(const quaternion<T>&);
    
    template<typename T>
    using quaternion_slerp_func = quaternion<T>(*)(const quaternion<T>&, const quaternion<T>&, T);
    
    // Octonion operation dispatchers
    template<typename T>
    using octonion_multiply_func = octonion<T>(*)(const octonion<T>&, const octonion<T>&);
    
    template<typename T>
    using octonion_normalize_func = octonion<T>(*)(const octonion<T>&);
    
    // Sedenion operation dispatchers
    template<typename T>
    using sedenion_multiply_func = sedenion<T>(*)(const sedenion<T>&, const sedenion<T>&);
    
    template<typename T>
    using sedenion_normalize_func = std::optional<sedenion<T>>(*)(const sedenion<T>&);
    
    // Get optimal implementations for current hardware
    template<typename T>
    quaternion_multiply_func<T> get_quaternion_multiply() noexcept;
    
    template<typename T>
    quaternion_normalize_func<T> get_quaternion_normalize() noexcept;
    
    template<typename T>
    quaternion_slerp_func<T> get_quaternion_slerp() noexcept;
    
    template<typename T>
    octonion_multiply_func<T> get_octonion_multiply() noexcept;
    
    template<typename T>
    octonion_normalize_func<T> get_octonion_normalize() noexcept;
    
    template<typename T>
    sedenion_multiply_func<T> get_sedenion_multiply() noexcept;
    
    template<typename T>
    sedenion_normalize_func<T> get_sedenion_normalize() noexcept;
}

/**
 * @brief High-level convenience functions with automatic optimization
 */
namespace functions {
    // Quaternion functions
    template<typename T>
    quaternion<T> quat_multiply(const quaternion<T>& a, const quaternion<T>& b) noexcept {
        static auto func = dispatch::get_quaternion_multiply<T>();
        return func(a, b);
    }
    
    template<typename T>
    quaternion<T> quat_normalize(const quaternion<T>& q) noexcept {
        static auto func = dispatch::get_quaternion_normalize<T>();
        return func(q);
    }
    
    template<typename T>
    quaternion<T> quat_slerp(const quaternion<T>& start, const quaternion<T>& end, T t) noexcept {
        static auto func = dispatch::get_quaternion_slerp<T>();
        return func(start, end, t);
    }
    
    template<typename T>
    std::array<T, 3> quat_rotate_vector(const quaternion<T>& q, const std::array<T, 3>& v) noexcept {
        return q.rotate_vector(v);
    }
    
    // Octonion functions
    template<typename T>
    octonion<T> oct_multiply(const octonion<T>& a, const octonion<T>& b) noexcept {
        static auto func = dispatch::get_octonion_multiply<T>();
        return func(a, b);
    }
    
    template<typename T>
    octonion<T> oct_normalize(const octonion<T>& o) noexcept {
        static auto func = dispatch::get_octonion_normalize<T>();
        return func(o);
    }
    
    template<typename T>
    octonion<T> oct_fano_multiply(const octonion<T>& a, const octonion<T>& b) noexcept {
        return a.fano_multiply(b);
    }
    
    // Sedenion functions
    template<typename T>
    sedenion<T> sed_multiply(const sedenion<T>& a, const sedenion<T>& b) noexcept {
        static auto func = dispatch::get_sedenion_multiply<T>();
        return func(a, b);
    }
    
    template<typename T>
    std::optional<sedenion<T>> sed_normalize(const sedenion<T>& s) noexcept {
        static auto func = dispatch::get_sedenion_normalize<T>();
        return func(s);
    }
    
    template<typename T>
    bool sed_is_zero_divisor(const sedenion<T>& s) noexcept {
        return s.is_zero_divisor();
    }
    
    // Conversion functions
    template<typename T>
    octonion<T> quat_to_octonion(const quaternion<T>& q) noexcept {
        return octonion<T>(q, quaternion<T>::zero());
    }
    
    template<typename T>
    sedenion<T> oct_to_sedenion(const octonion<T>& o) noexcept {
        return sedenion<T>(o, octonion<T>::zero());
    }
    
    template<typename T>
    sedenion<T> quat_to_sedenion(const quaternion<T>& q) noexcept {
        return oct_to_sedenion(quat_to_octonion(q));
    }
}

/**
 * @brief Batch processing utilities
 */
namespace batch_processing {
    // Batch quaternion operations
    template<typename T>
    void quaternion_multiply_batch(const quaternion<T>* a, const quaternion<T>* b,
                                  quaternion<T>* result, std::size_t count) noexcept {
        batch::multiply(a, b, result, count);
    }
    
    template<typename T>
    void quaternion_normalize_batch(const quaternion<T>* input, quaternion<T>* output,
                                   std::size_t count) noexcept {
        batch::normalize(input, output, count);
    }
    
    template<typename T>
    void quaternion_slerp_batch(const quaternion<T>* start, const quaternion<T>* end,
                               T t, quaternion<T>* result, std::size_t count) noexcept {
        batch::slerp(start, end, t, result, count);
    }
    
    // Batch octonion operations
    template<typename T>
    void octonion_multiply_batch(const octonion<T>* a, const octonion<T>* b,
                                octonion<T>* result, std::size_t count) noexcept {
        batch::multiply(a, b, result, count);
    }
    
    template<typename T>
    void octonion_normalize_batch(const octonion<T>* input, octonion<T>* output,
                                 std::size_t count) noexcept {
        batch::normalize(input, output, count);
    }
    
    // Batch sedenion operations
    template<typename T>
    void sedenion_multiply_batch(const sedenion<T>* a, const sedenion<T>* b,
                                sedenion<T>* result, std::size_t count) noexcept {
        batch::multiply(a, b, result, count);
    }
    
    template<typename T>
    void sedenion_normalize_batch(const sedenion<T>* input, sedenion<T>* output,
                                 bool* success_flags, std::size_t count) noexcept {
        batch::normalize(input, output, success_flags, count);
    }
}

/**
 * @brief Performance monitoring and profiling
 */
namespace profiling {
    struct operation_stats {
        std::uint64_t calls;
        std::uint64_t total_cycles;
        std::uint64_t min_cycles;
        std::uint64_t max_cycles;
        double average_cycles;
    };
    
    class Profiler {
    public:
        static void enable() noexcept;
        static void disable() noexcept;
        static void reset() noexcept;
        
        static operation_stats get_quaternion_multiply_stats() noexcept;
        static operation_stats get_octonion_multiply_stats() noexcept;
        static operation_stats get_sedenion_multiply_stats() noexcept;
        
        static void print_report() noexcept;
        static void save_report(const char* filename) noexcept;
        
    private:
        static inline bool enabled_ = false;
        static inline operation_stats quat_multiply_stats_{};
        static inline operation_stats oct_multiply_stats_{};
        static inline operation_stats sed_multiply_stats_{};
    };
}

/**
 * @brief Testing and validation utilities
 */
namespace testing {
    template<typename T>
    bool validate_quaternion_properties(const quaternion<T>& q, T tolerance = T{1e-6}) noexcept;
    
    template<typename T>
    bool validate_octonion_properties(const octonion<T>& o, T tolerance = T{1e-6}) noexcept;
    
    template<typename T>
    bool validate_sedenion_properties(const sedenion<T>& s, T tolerance = T{1e-6}) noexcept;
    
    // Cross-implementation validation
    template<typename T>
    bool compare_implementations(const char* operation_name) noexcept;
    
    // Numerical accuracy tests
    template<typename T>
    T compute_numerical_error(const char* operation_name, std::size_t iterations) noexcept;
    
    // Performance comparison
    template<typename T>
    void benchmark_all_implementations(const char* operation_name) noexcept;
}

/**
 * @brief Legacy compatibility layer
 */
namespace compat {
    // Compatibility with old kernel quaternion_spinlock.hpp
    namespace spinlock {
        using atomic_quaternion_lock = atomic_quaternion<float>;
        
        class QuaternionSpinlock {
        private:
            atomic_quaternion_lock lock_;
            
        public:
            void lock() noexcept { lock_.lock(); }
            void unlock() noexcept { lock_.unlock(); }
            bool try_lock() noexcept { return lock_.try_lock(); }
        };
    }
    
    // Compatibility with old common/math implementations
    namespace legacy {
        template<typename T>
        using Quaternion = quaternion<T>;
        
        template<typename T>
        using Octonion = octonion<T>;
        
        template<typename T>
        using Sedenion = sedenion<T>;
    }
}

} // namespace xinim::simd::math

// Auto-initialization on first use
namespace {
    struct MathLibraryInitializer {
        MathLibraryInitializer() {
            xinim::simd::math::MathLibrary::initialize();
        }
    };
    
    static MathLibraryInitializer init_math_library;
}

// Include all implementation details
#include "quaternion_impl.hpp"
#include "octonion_impl.hpp" 
#include "sedenion_impl.hpp"
