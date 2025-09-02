/**
 * @file core.hpp
 * @brief Core SIMD abstraction layer for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Unified SIMD interface supporting all major instruction sets:
 * - x86-64: X87, MMX, 3DNow!, SSE1-4.2, FMA, AVX, AVX2, AVX-512
 * - ARM: VFPv3/4, NEON, SVE, SVE2
 * - RISC-V: Vector extensions (future)
 * 
 * Features:
 * - Runtime feature detection
 * - Compile-time instruction set selection
 * - Fallback implementations for unsupported hardware
 * - Type-safe vector operations
 * - Performance counters and profiling
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <concepts>
#include <bit>
#include <array>

namespace xinim::simd {

/**
 * @brief SIMD instruction set capabilities
 */
enum class Capability : std::uint64_t {
    // x86-64 instruction sets
    X87_FPU       = 0x0000000000000001ULL,
    MMX           = 0x0000000000000002ULL,
    SSE           = 0x0000000000000004ULL,
    SSE2          = 0x0000000000000008ULL,
    SSE3          = 0x0000000000000010ULL,
    SSSE3         = 0x0000000000000020ULL,
    SSE4_1        = 0x0000000000000040ULL,
    SSE4_2        = 0x0000000000000080ULL,
    SSE4A         = 0x0000000000000100ULL,
    FMA3          = 0x0000000000000200ULL,
    FMA4          = 0x0000000000000400ULL,
    AVX           = 0x0000000000000800ULL,
    AVX2          = 0x0000000000001000ULL,
    AVX512F       = 0x0000000000002000ULL,
    AVX512VL      = 0x0000000000004000ULL,
    AVX512BW      = 0x0000000000008000ULL,
    AVX512DQ      = 0x0000000000010000ULL,
    AVX512CD      = 0x0000000000020000ULL,
    AVX512ER      = 0x0000000000040000ULL,
    AVX512PF      = 0x0000000000080000ULL,
    AVX512VNNI    = 0x0000000000100000ULL,
    AMD_3DNOW     = 0x0000000000200000ULL,
    AMD_3DNOWEXT  = 0x0000000000400000ULL,
    
    // ARM instruction sets
    VFP           = 0x0000000100000000ULL,
    VFP3          = 0x0000000200000000ULL,
    VFP4          = 0x0000000400000000ULL,
    NEON          = 0x0000000800000000ULL,
    NEON_FP16     = 0x0000001000000000ULL,
    CRYPTO        = 0x0000002000000000ULL,
    CRC32         = 0x0000004000000000ULL,
    SVE           = 0x0000008000000000ULL,
    SVE2          = 0x0000010000000000ULL,
    
    // RISC-V extensions (future)
    RV_V          = 0x0001000000000000ULL,
    RV_ZVL128B    = 0x0002000000000000ULL,
    RV_ZVL256B    = 0x0004000000000000ULL,
    RV_ZVL512B    = 0x0008000000000000ULL,
    
    // Special capabilities
    UNALIGNED_LOAD = 0x1000000000000000ULL,
    FAST_GATHER    = 0x2000000000000000ULL,
    FAST_SCATTER   = 0x4000000000000000ULL,
    PREFETCH       = 0x8000000000000000ULL
};

/**
 * @brief Vector width constants
 */
namespace width {
    inline constexpr std::size_t x64  = 64;
    inline constexpr std::size_t x128 = 128;
    inline constexpr std::size_t x256 = 256;
    inline constexpr std::size_t x512 = 512;
    inline constexpr std::size_t x1024 = 1024;
    inline constexpr std::size_t x2048 = 2048;
}

/**
 * @brief SIMD vector types with compile-time width
 */
template<typename T, std::size_t Width>
    requires std::is_arithmetic_v<T>
struct alignas(Width / 8) Vector {
    static constexpr std::size_t width_bits = Width;
    static constexpr std::size_t width_bytes = Width / 8;
    static constexpr std::size_t element_count = Width / (sizeof(T) * 8);
    
    using element_type = T;
    using storage_type = std::array<T, element_count>;
    
    storage_type data;
    
    // Constructors
    constexpr Vector() = default;
    
    constexpr explicit Vector(T scalar) noexcept {
        data.fill(scalar);
    }
    
    constexpr Vector(const storage_type& arr) noexcept : data(arr) {}
    
    template<typename... Args>
        requires (sizeof...(Args) == element_count) && (std::convertible_to<Args, T> && ...)
    constexpr Vector(Args... args) noexcept : data{static_cast<T>(args)...} {}
    
    // Element access
    constexpr T& operator[](std::size_t index) noexcept { return data[index]; }
    constexpr const T& operator[](std::size_t index) const noexcept { return data[index]; }
    
    // Iterator support
    constexpr auto begin() noexcept { return data.begin(); }
    constexpr auto end() noexcept { return data.end(); }
    constexpr auto begin() const noexcept { return data.begin(); }
    constexpr auto end() const noexcept { return data.end(); }
    
    // Size information
    static constexpr std::size_t size() noexcept { return element_count; }
    static constexpr std::size_t byte_size() noexcept { return width_bytes; }
};

/**
 * @brief Common vector type aliases
 */
using v64i8   = Vector<std::int8_t,   width::x64>;
using v64u8   = Vector<std::uint8_t,  width::x64>;
using v64i16  = Vector<std::int16_t,  width::x64>;
using v64u16  = Vector<std::uint16_t, width::x64>;
using v64i32  = Vector<std::int32_t,  width::x64>;
using v64u32  = Vector<std::uint32_t, width::x64>;
using v64f32  = Vector<float,         width::x64>;

using v128i8  = Vector<std::int8_t,   width::x128>;
using v128u8  = Vector<std::uint8_t,  width::x128>;
using v128i16 = Vector<std::int16_t,  width::x128>;
using v128u16 = Vector<std::uint16_t, width::x128>;
using v128i32 = Vector<std::int32_t,  width::x128>;
using v128u32 = Vector<std::uint32_t, width::x128>;
using v128i64 = Vector<std::int64_t,  width::x128>;
using v128u64 = Vector<std::uint64_t, width::x128>;
using v128f32 = Vector<float,         width::x128>;
using v128f64 = Vector<double,        width::x128>;

using v256i8  = Vector<std::int8_t,   width::x256>;
using v256u8  = Vector<std::uint8_t,  width::x256>;
using v256i16 = Vector<std::int16_t,  width::x256>;
using v256u16 = Vector<std::uint16_t, width::x256>;
using v256i32 = Vector<std::int32_t,  width::x256>;
using v256u32 = Vector<std::uint32_t, width::x256>;
using v256i64 = Vector<std::int64_t,  width::x256>;
using v256u64 = Vector<std::uint64_t, width::x256>;
using v256f32 = Vector<float,         width::x256>;
using v256f64 = Vector<double,        width::x256>;

using v512i8  = Vector<std::int8_t,   width::x512>;
using v512u8  = Vector<std::uint8_t,  width::x512>;
using v512i16 = Vector<std::int16_t,  width::x512>;
using v512u16 = Vector<std::uint16_t, width::x512>;
using v512i32 = Vector<std::int32_t,  width::x512>;
using v512u32 = Vector<std::uint32_t, width::x512>;
using v512i64 = Vector<std::int64_t,  width::x512>;
using v512u64 = Vector<std::uint64_t, width::x512>;
using v512f32 = Vector<float,         width::x512>;
using v512f64 = Vector<double,        width::x512>;

/**
 * @brief SIMD runtime capabilities detection
 */
class CapabilityDetector {
private:
    std::uint64_t capabilities_{0};
    bool detected_{false};
    
public:
    /**
     * @brief Detects available SIMD capabilities
     */
    void detect() noexcept;
    
    /**
     * @brief Checks if a specific capability is available
     * @param cap Capability to check
     * @return true if available
     */
    [[nodiscard]] bool has(Capability cap) const noexcept {
        return (capabilities_ & static_cast<std::uint64_t>(cap)) != 0;
    }
    
    /**
     * @brief Checks if all specified capabilities are available
     * @param caps Capabilities to check
     * @return true if all are available
     */
    template<typename... Caps>
    [[nodiscard]] bool has_all(Caps... caps) const noexcept {
        return (has(caps) && ...);
    }
    
    /**
     * @brief Checks if any of the specified capabilities are available
     * @param caps Capabilities to check
     * @return true if any are available
     */
    template<typename... Caps>
    [[nodiscard]] bool has_any(Caps... caps) const noexcept {
        return (has(caps) || ...);
    }
    
    /**
     * @brief Gets the maximum vector width supported
     * @return Maximum width in bits
     */
    [[nodiscard]] std::size_t max_vector_width() const noexcept;
    
    /**
     * @brief Gets the optimal vector width for the given type
     * @tparam T Element type
     * @return Optimal width in bits
     */
    template<typename T>
    [[nodiscard]] std::size_t optimal_width() const noexcept;
    
    /**
     * @brief Gets a string description of available capabilities
     * @return Human-readable capability list
     */
    [[nodiscard]] const char* description() const noexcept;
    
    /**
     * @brief Gets the global capability detector instance
     * @return Reference to the global detector
     */
    static CapabilityDetector& instance() noexcept {
        static CapabilityDetector detector;
        if (!detector.detected_) {
            detector.detect();
            detector.detected_ = true;
        }
        return detector;
    }
};

/**
 * @brief Concept for SIMD vector types
 */
template<typename T>
concept SimdVector = requires {
    typename T::element_type;
    typename T::storage_type;
    T::width_bits;
    T::width_bytes; 
    T::element_count;
};

/**
 * @brief Concept for arithmetic SIMD vectors
 */
template<typename T>
concept ArithmeticSimdVector = SimdVector<T> && std::is_arithmetic_v<typename T::element_type>;

/**
 * @brief Performance profiler for SIMD operations
 */
class SimdProfiler {
private:
    struct OpStats {
        std::uint64_t call_count{0};
        std::uint64_t total_cycles{0};
        std::uint64_t total_elements{0};
    };
    
    std::array<OpStats, 256> op_stats_{};
    
public:
    /**
     * @brief Records operation statistics
     * @param op_id Operation identifier
     * @param cycles CPU cycles consumed
     * @param elements Number of elements processed
     */
    void record_operation(std::uint8_t op_id, std::uint64_t cycles, std::uint64_t elements) noexcept;
    
    /**
     * @brief Gets average cycles per element for an operation
     * @param op_id Operation identifier
     * @return Average cycles per element
     */
    [[nodiscard]] double avg_cycles_per_element(std::uint8_t op_id) const noexcept;
    
    /**
     * @brief Gets operation statistics
     * @param op_id Operation identifier
     * @return Operation statistics
     */
    [[nodiscard]] const OpStats& get_stats(std::uint8_t op_id) const noexcept;
    
    /**
     * @brief Resets all statistics
     */
    void reset() noexcept;
};

/**
 * @brief RAII profiler scope
 */
class ProfileScope {
private:
    std::uint8_t op_id_;
    std::uint64_t start_cycles_;
    std::uint64_t element_count_;
    
public:
    explicit ProfileScope(std::uint8_t op_id, std::uint64_t element_count = 0) noexcept;
    ~ProfileScope() noexcept;
    
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

/**
 * @brief Macro for easy profiling
 */
#define XINIM_SIMD_PROFILE(op_id, elements) \
    xinim::simd::ProfileScope _prof_scope((op_id), (elements))

/**
 * @brief Generic SIMD operation dispatcher
 */
template<typename Op, typename... Args>
auto dispatch_simd_op(Args&&... args) -> decltype(Op::template execute<Args...>(std::forward<Args>(args)...)) {
    auto& detector = CapabilityDetector::instance();
    
    // Try most optimal implementation first
    if constexpr (Op::template supports<Capability::AVX512F>()) {
        if (detector.has(Capability::AVX512F)) {
            return Op::template execute_avx512(std::forward<Args>(args)...);
        }
    }
    
    if constexpr (Op::template supports<Capability::AVX2>()) {
        if (detector.has(Capability::AVX2)) {
            return Op::template execute_avx2(std::forward<Args>(args)...);
        }
    }
    
    if constexpr (Op::template supports<Capability::AVX>()) {
        if (detector.has(Capability::AVX)) {
            return Op::template execute_avx(std::forward<Args>(args)...);
        }
    }
    
    if constexpr (Op::template supports<Capability::SSE2>()) {
        if (detector.has(Capability::SSE2)) {
            return Op::template execute_sse2(std::forward<Args>(args)...);
        }
    }
    
    if constexpr (Op::template supports<Capability::NEON>()) {
        if (detector.has(Capability::NEON)) {
            return Op::template execute_neon(std::forward<Args>(args)...);
        }
    }
    
    // Fallback to scalar implementation
    return Op::template execute_scalar(std::forward<Args>(args)...);
}

} // namespace xinim::simd
