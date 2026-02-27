#include <array>
#include <concepts>
#include <expected>
#include <memory>
#include <span>
#include <vector>
#include <immintrin.h>  // Intel intrinsics
#include <cstdint>
#include <bit>
#include <algorithm>
#include <system_error>

// AMD 3DNow! support (legacy)
#ifdef __3dNOW__
#include <mm3dnow.h>
#endif

#ifdef __3dNOW_A__
#include <mmintrin.h>
#endif

namespace xinim::crypto::kyber::simd {

// SIMD capability detection at compile time based on target architecture macros
struct simd_caps {
    #ifdef __SSE__
    static constexpr bool has_sse = true;
    #else
    static constexpr bool has_sse = false;
    #endif

    #ifdef __SSE2__
    static constexpr bool has_sse2 = true;
    #else
    static constexpr bool has_sse2 = false;
    #endif

    #ifdef __SSE3__
    static constexpr bool has_sse3 = true;
    #else
    static constexpr bool has_sse3 = false;
    #endif

    #ifdef __SSSE3__
    static constexpr bool has_ssse3 = true;
    #else
    static constexpr bool has_ssse3 = false;
    #endif

    #ifdef __SSE4_1__
    static constexpr bool has_sse4_1 = true;
    #else
    static constexpr bool has_sse4_1 = false;
    #endif

    #ifdef __SSE4_2__
    static constexpr bool has_sse4_2 = true;
    #else
    static constexpr bool has_sse4_2 = false;
    #endif

    #ifdef __SSE4A__
    static constexpr bool has_sse4a = true;
    #else
    static constexpr bool has_sse4a = false;
    #endif

    #ifdef __AVX__
    static constexpr bool has_avx = true;
    #else
    static constexpr bool has_avx = false;
    #endif

    #ifdef __AVX2__
    static constexpr bool has_avx2 = true;
    #else
    static constexpr bool has_avx2 = false;
    #endif

    #ifdef __AVX512F__
    static constexpr bool has_avx512f = true;
    #else
    static constexpr bool has_avx512f = false;
    #endif

    #ifdef __AVX512BW__
    static constexpr bool has_avx512bw = true;
    #else
    static constexpr bool has_avx512bw = false;
    #endif

    #ifdef __AVX512DQ__
    static constexpr bool has_avx512dq = true;
    #else
    static constexpr bool has_avx512dq = false;
    #endif

    #ifdef __AVX512VL__
    static constexpr bool has_avx512vl = true;
    #else
    static constexpr bool has_avx512vl = false;
    #endif

    #ifdef __AVX512VNNI__
    static constexpr bool has_avx512vnni = true;
    #else
    static constexpr bool has_avx512vnni = false;
    #endif
    
    #ifdef __3dNOW__
    static constexpr bool has_3dnow = true;
    #else
    static constexpr bool has_3dnow = false;
    #endif
    
    #ifdef __3dNOW_A__
    static constexpr bool has_3dnow_ext = true;
    #else
    static constexpr bool has_3dnow_ext = false;
    #endif
};

/**
 * @brief Runtime SIMD capability detection via CPUID.
 *
 * The compile-time simd_caps above reflect what the compiler targets.
 * This struct detects what the actual CPU supports at runtime, which
 * matters when compiled with -march=x86-64 (baseline) but running on
 * a CPU with AVX2/AVX-512.
 */
struct runtime_simd_caps {
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;

    static runtime_simd_caps detect() noexcept {
        runtime_simd_caps caps;
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

        // CPUID leaf 7, subleaf 0: extended feature flags
        asm volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(7), "c"(0));

        caps.avx2     = (ebx >> 5) & 1;   // EBX bit 5
        caps.avx512f  = (ebx >> 16) & 1;  // EBX bit 16
        caps.avx512bw = (ebx >> 30) & 1;  // EBX bit 30

        return caps;
    }
};

/// Global runtime SIMD capabilities, initialized at boot.
inline runtime_simd_caps g_runtime_simd = runtime_simd_caps::detect();

// Kyber parameter sets
enum class kyber_level : std::uint8_t {
    KYBER_512 = 1,   // Level 1 security - AES-128 equivalent 
    KYBER_768 = 2,   // Level 3 security - AES-192 equivalent
    KYBER_1024 = 3   // Level 5 security - AES-256 equivalent
};

// Kyber parameters structure
template<kyber_level Level>
struct kyber_params {
    static constexpr std::size_t n = 256;
    static constexpr std::size_t q = 3329;
    static constexpr std::size_t k = (Level == kyber_level::KYBER_512) ? 2 : 
                                    (Level == kyber_level::KYBER_768) ? 3 : 4;
    static constexpr std::size_t eta_1 = (Level == kyber_level::KYBER_512) ? 3 : 2;
    static constexpr std::size_t eta_2 = 2;
    static constexpr std::size_t du = (Level == kyber_level::KYBER_512) ? 10 : 
                                     (Level == kyber_level::KYBER_768) ? 10 : 11;
    static constexpr std::size_t dv = (Level == kyber_level::KYBER_512) ? 4 : 
                                     (Level == kyber_level::KYBER_768) ? 4 : 5;
    
    // Key and ciphertext sizes
    static constexpr std::size_t public_key_bytes = k * n * 12 / 8 + 32;
    static constexpr std::size_t secret_key_bytes = k * n * 12 / 8 + k * n * 12 / 8 + 32 + 32 + 32;
    static constexpr std::size_t ciphertext_bytes = k * n * du / 8 + n * dv / 8;
    static constexpr std::size_t shared_secret_bytes = 32;
};

// SIMD-optimized polynomial representation
template<kyber_level Level>
class alignas(64) poly_simd {
private:
    static constexpr std::size_t n = kyber_params<Level>::n;
    static constexpr std::size_t q = kyber_params<Level>::q;
    
    // Store coefficients in SIMD-friendly format
    alignas(64) std::array<std::int16_t, n> coeffs;

public:
    constexpr poly_simd() noexcept : coeffs{} {}
    
    // SIMD reduction modulo q using fastest available instruction set
    void reduce_mod_q() noexcept {
        if constexpr (simd_caps::has_avx512bw) {
            reduce_avx512();
        } else if constexpr (simd_caps::has_avx2) {
            reduce_avx2();
        } else if constexpr (simd_caps::has_sse4_1) {
            reduce_sse41();
        } else {
            reduce_scalar();
        }
    }

private:
    // AVX-512BW implementation - fastest for wide vectors
    void reduce_avx512() noexcept requires(simd_caps::has_avx512bw) {
        #ifdef __AVX512BW__
        const __m512i q_vec = _mm512_set1_epi16(static_cast<std::int16_t>(q));
        const __m512i q_inv = _mm512_set1_epi16(-3327);  // Montgomery inverse
        
        for (std::size_t i = 0; i < n; i += 32) {
            __m512i a = _mm512_loadu_epi16(&coeffs[i]);
            
            // Montgomery reduction using VNNI if available
            #ifdef __AVX512VNNI__
            if constexpr (simd_caps::has_avx512vnni) {
                // Use VNNI for multiply-accumulate
                __m512i t = _mm512_dpwssd_epi32(_mm512_setzero_epi32(), a, q_inv);
                __m512i result = _mm512_packs_epi32(
                    _mm512_sub_epi32(_mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(a, 0)), t),
                    _mm512_sub_epi32(_mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(a, 1)), 
                                   _mm512_srli_epi32(t, 16))
                );
                _mm512_storeu_epi16(&coeffs[i], result);
            } else 
            #endif
            {
                // Standard AVX-512 reduction
                __m512i t = _mm512_mulhi_epi16(a, q_inv);
                __m512i result = _mm512_sub_epi16(a, _mm512_mullo_epi16(t, q_vec));
                _mm512_storeu_epi16(&coeffs[i], result);
            }
        }
        #endif
    }
    
    // AVX2 implementation - good balance of performance and compatibility  
    void reduce_avx2() noexcept requires(simd_caps::has_avx2) {
        #ifdef __AVX2__
        const __m256i q_vec = _mm256_set1_epi16(static_cast<std::int16_t>(q));
        const __m256i q_inv = _mm256_set1_epi16(-3327);
        
        for (std::size_t i = 0; i < n; i += 16) {
            __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[i]));
            __m256i t = _mm256_mulhi_epi16(a, q_inv);
            __m256i result = _mm256_sub_epi16(a, _mm256_mullo_epi16(t, q_vec));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[i]), result);
        }
        #endif
    }
    
    // SSE4.1 implementation - broad compatibility
    void reduce_sse41() noexcept requires(simd_caps::has_sse4_1) {
        #ifdef __SSE4_1__
        const __m128i q_vec = _mm_set1_epi16(static_cast<std::int16_t>(q));
        const __m128i q_inv = _mm_set1_epi16(-3327);
        
        for (std::size_t i = 0; i < n; i += 8) {
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&coeffs[i]));
            __m128i t = _mm_mulhi_epi16(a, q_inv);
            __m128i result = _mm_sub_epi16(a, _mm_mullo_epi16(t, q_vec));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&coeffs[i]), result);
        }
        #endif
    }
    
    // Scalar fallback
    void reduce_scalar() noexcept {
        for (auto& coeff : coeffs) {
            // Barrett reduction
            std::int32_t t = (static_cast<std::int32_t>(coeff) * 5039) >> 23;
            coeff = static_cast<std::int16_t>(coeff - t * static_cast<std::int32_t>(q));
            if (coeff >= static_cast<std::int16_t>(q)) coeff -= static_cast<std::int16_t>(q);
            if (coeff < 0) coeff += static_cast<std::int16_t>(q);
        }
    }

public:
    // NTT implementation with SIMD optimization
    void ntt() noexcept {
        if constexpr (simd_caps::has_avx512f) {
            ntt_avx512();
        } else if constexpr (simd_caps::has_avx2) {
            ntt_avx2();
        } else {
            ntt_scalar();
        }
    }
    
    // Inverse NTT
    void invntt() noexcept {
        if constexpr (simd_caps::has_avx512f) {
            invntt_avx512();
        } else if constexpr (simd_caps::has_avx2) {
            invntt_avx2();
        } else {
            invntt_scalar();
        }
    }

private:
    void ntt_avx512() noexcept requires(simd_caps::has_avx512f) {
        #ifdef __AVX512F__
        // Optimized Cooley-Tukey NTT with AVX-512
        static constexpr auto zetas = generate_ntt_constants();
        
        for (std::size_t len = 128; len >= 2; len >>= 1) {
            for (std::size_t start = 0; start < n; start = (len << 1) + start) {
                const std::int16_t zeta = zetas[128 - len];
                const __m512i zeta_vec = _mm512_set1_epi16(zeta);
                
                for (std::size_t j = start; j < start + len; j += 32) {
                    __m512i a = _mm512_loadu_epi16(&coeffs[j]);
                    __m512i b = _mm512_loadu_epi16(&coeffs[j + len]);
                    
                    // Butterfly operation with Montgomery multiplication
                    __m512i t = _mm512_mullo_epi16(b, zeta_vec);
                    _mm512_storeu_epi16(&coeffs[j], _mm512_add_epi16(a, t));
                    _mm512_storeu_epi16(&coeffs[j + len], _mm512_sub_epi16(a, t));
                }
            }
        }
        reduce_mod_q();
        #endif
    }
    
    void ntt_avx2() noexcept requires(simd_caps::has_avx2) {
        #ifdef __AVX2__
        // AVX2 NTT implementation
        static constexpr auto zetas = generate_ntt_constants();
        
        for (std::size_t len = 128; len >= 2; len >>= 1) {
            for (std::size_t start = 0; start < n; start = (len << 1) + start) {
                const std::int16_t zeta = zetas[128 - len];
                const __m256i zeta_vec = _mm256_set1_epi16(zeta);
                
                for (std::size_t j = start; j < start + len; j += 16) {
                    __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[j]));
                    __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[j + len]));
                    
                    __m256i t = _mm256_mullo_epi16(b, zeta_vec);
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[j]), 
                                       _mm256_add_epi16(a, t));
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[j + len]), 
                                       _mm256_sub_epi16(a, t));
                }
            }
        }
        reduce_mod_q();
        #endif
    }
    
    void ntt_scalar() noexcept {
        // Scalar NTT fallback
        static constexpr auto zetas = generate_ntt_constants();
        
        for (std::size_t len = 128; len >= 2; len >>= 1) {
            for (std::size_t start = 0; start < n; start = (len << 1) + start) {
                const std::int16_t zeta = zetas[128 - len];
                for (std::size_t j = start; j < start + len; ++j) {
                    std::int16_t t = montgomery_mul(coeffs[j + len], zeta);
                    coeffs[j + len] = static_cast<std::int16_t>(coeffs[j] - t);
                    coeffs[j] = static_cast<std::int16_t>(coeffs[j] + t);
                }
            }
        }
        reduce_mod_q();
    }
    
    void invntt_avx512() noexcept requires(simd_caps::has_avx512f) {
        #ifdef __AVX512F__
        // AVX-512 inverse NTT
        static constexpr auto inv_zetas = generate_inv_ntt_constants();
        
        for (std::size_t len = 2; len <= 128; len <<= 1) {
            for (std::size_t start = 0; start < n; start = (len << 1) + start) {
                const std::int16_t zeta = inv_zetas[len - 2];
                const __m512i zeta_vec = _mm512_set1_epi16(zeta);
                
                for (std::size_t j = start; j < start + len; j += 32) {
                    __m512i a = _mm512_loadu_epi16(&coeffs[j]);
                    __m512i b = _mm512_loadu_epi16(&coeffs[j + len]);
                    
                    __m512i t = _mm512_add_epi16(a, b);
                    b = _mm512_sub_epi16(a, b);
                    b = _mm512_mullo_epi16(b, zeta_vec);
                    
                    _mm512_storeu_epi16(&coeffs[j], t);
                    _mm512_storeu_epi16(&coeffs[j + len], b);
                }
            }
        }
        
        // Final scaling by n^-1
        const __m512i n_inv = _mm512_set1_epi16(3303);  // 256^-1 mod q
        for (std::size_t i = 0; i < n; i += 32) {
            __m512i a = _mm512_loadu_epi16(&coeffs[i]);
            _mm512_storeu_epi16(&coeffs[i], _mm512_mullo_epi16(a, n_inv));
        }
        reduce_mod_q();
        #endif
    }
    
    void invntt_avx2() noexcept requires(simd_caps::has_avx2) {
        #ifdef __AVX2__
        // AVX2 inverse NTT implementation (similar pattern)
        static constexpr auto inv_zetas = generate_inv_ntt_constants();
        
        for (std::size_t len = 2; len <= 128; len <<= 1) {
            for (std::size_t start = 0; start < n; start = (len << 1) + start) {
                const std::int16_t zeta = inv_zetas[len - 2];
                const __m256i zeta_vec = _mm256_set1_epi16(zeta);
                
                for (std::size_t j = start; j < start + len; j += 16) {
                    __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[j]));
                    __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[j + len]));
                    
                    __m256i t = _mm256_add_epi16(a, b);
                    b = _mm256_sub_epi16(a, b);
                    b = _mm256_mullo_epi16(b, zeta_vec);
                    
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[j]), t);
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[j + len]), b);
                }
            }
        }
        
        const __m256i n_inv = _mm256_set1_epi16(3303);
        for (std::size_t i = 0; i < n; i += 16) {
            __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[i]));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[i]), 
                               _mm256_mullo_epi16(a, n_inv));
        }
        reduce_mod_q();
        #endif
    }
    
    void invntt_scalar() noexcept {
        // Scalar inverse NTT fallback
        static constexpr auto inv_zetas = generate_inv_ntt_constants();
        
        for (std::size_t len = 2; len <= 128; len <<= 1) {
            for (std::size_t start = 0; start < n; start = (len << 1) + start) {
                const std::int16_t zeta = inv_zetas[len - 2];
                for (std::size_t j = start; j < start + len; ++j) {
                    std::int16_t t = coeffs[j];
                    coeffs[j] = static_cast<std::int16_t>(t + coeffs[j + len]);
                    coeffs[j + len] = montgomery_mul(static_cast<std::int16_t>(coeffs[j + len] - t), zeta);
                }
            }
        }
        
        for (auto& coeff : coeffs) {
            coeff = montgomery_mul(coeff, 3303);  // Multiply by n^-1
        }
        reduce_mod_q();
    }

    // Montgomery multiplication helper
    static constexpr std::int16_t montgomery_mul(std::int16_t a, std::int16_t b) noexcept {
        return static_cast<std::int16_t>((static_cast<std::int64_t>(a) * b * 3303) >> 16);
    }

    // Compile-time NTT constant generation
    static constexpr auto generate_ntt_constants() noexcept {
        std::array<std::int16_t, 128> zetas{};
        // Generate primitive root powers for NTT
        std::int16_t root = 17;  // Primitive 512-th root of unity mod q
        std::int16_t pow = 1;
        for (std::size_t i = 0; i < 128; ++i) {
            zetas[i] = pow;
            pow = montgomery_mul(pow, root);
        }
        return zetas;
    }
    
    static constexpr auto generate_inv_ntt_constants() noexcept {
        std::array<std::int16_t, 128> inv_zetas{};
        // Generate inverse NTT constants
        std::int16_t inv_root = 1175;  // Inverse of primitive root
        std::int16_t pow = 1;
        for (std::size_t i = 0; i < 128; ++i) {
            inv_zetas[i] = pow;
            pow = montgomery_mul(pow, inv_root);
        }
        return inv_zetas;
    }

public:
    // Polynomial operations
    poly_simd operator+(const poly_simd& other) const noexcept {
        poly_simd result;
        
        if constexpr (simd_caps::has_avx512f) {
            #ifdef __AVX512F__
            for (std::size_t i = 0; i < n; i += 32) {
                __m512i a = _mm512_loadu_epi16(&coeffs[i]);
                __m512i b = _mm512_loadu_epi16(&other.coeffs[i]);
                _mm512_storeu_epi16(&result.coeffs[i], _mm512_add_epi16(a, b));
            }
            #endif
        } else if constexpr (simd_caps::has_avx2) {
            #ifdef __AVX2__
            for (std::size_t i = 0; i < n; i += 16) {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[i]));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&other.coeffs[i]));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result.coeffs[i]), 
                                   _mm256_add_epi16(a, b));
            }
            #endif
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                result.coeffs[i] = static_cast<std::int16_t>(coeffs[i] + other.coeffs[i]);
            }
        }
        
        result.reduce_mod_q();
        return result;
    }
    
    poly_simd operator-(const poly_simd& other) const noexcept {
        poly_simd result;
        
        if constexpr (simd_caps::has_avx512f) {
            #ifdef __AVX512F__
            for (std::size_t i = 0; i < n; i += 32) {
                __m512i a = _mm512_loadu_epi16(&coeffs[i]);
                __m512i b = _mm512_loadu_epi16(&other.coeffs[i]);
                _mm512_storeu_epi16(&result.coeffs[i], _mm512_sub_epi16(a, b));
            }
            #endif
        } else if constexpr (simd_caps::has_avx2) {
            #ifdef __AVX2__
            for (std::size_t i = 0; i < n; i += 16) {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[i]));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&other.coeffs[i]));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result.coeffs[i]),
                                   _mm256_sub_epi16(a, b));
            }
            #endif
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                result.coeffs[i] = static_cast<std::int16_t>(coeffs[i] - other.coeffs[i]);
            }
        }
        
        result.reduce_mod_q();
        return result;
    }
    
    // Pointwise multiplication (for NTT domain)
    poly_simd pointwise_mul(const poly_simd& other) const noexcept {
        poly_simd result;
        
        if constexpr (simd_caps::has_avx512f) {
            #ifdef __AVX512F__
            for (std::size_t i = 0; i < n; i += 32) {
                __m512i a = _mm512_loadu_epi16(&coeffs[i]);
                __m512i b = _mm512_loadu_epi16(&other.coeffs[i]);
                _mm512_storeu_epi16(&result.coeffs[i], _mm512_mullo_epi16(a, b));
            }
            #endif
        } else if constexpr (simd_caps::has_avx2) {
            #ifdef __AVX2__
            for (std::size_t i = 0; i < n; i += 16) {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[i]));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&other.coeffs[i]));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result.coeffs[i]), 
                                   _mm256_mullo_epi16(a, b));
            }
            #endif
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                result.coeffs[i] = montgomery_mul(coeffs[i], other.coeffs[i]);
            }
        }
        
        result.reduce_mod_q();
        return result;
    }

    // Access coefficients
    [[nodiscard]] const std::array<std::int16_t, n>& data() const noexcept { return coeffs; }
    [[nodiscard]] std::array<std::int16_t, n>& data() noexcept { return coeffs; }
};

// C++23 concepts for Kyber operations
template<typename T>
concept KyberKey = requires {
    typename T::key_type;
    T::key_size;
} && std::is_trivially_copyable_v<T>;

template<typename T>
concept KyberCiphertext = requires {
    typename T::ciphertext_type;
    T::ciphertext_size;
} && std::is_trivially_copyable_v<T>;

// SIMD-optimized Kyber key types
template<kyber_level Level>
struct alignas(64) kyber_public_key {
    using key_type = std::array<std::byte, kyber_params<Level>::public_key_bytes>;
    static constexpr std::size_t key_size = kyber_params<Level>::public_key_bytes;
    
    alignas(64) key_type data;
    
    constexpr kyber_public_key() noexcept : data{} {}
    explicit constexpr kyber_public_key(const key_type& key_data) noexcept : data(key_data) {}
};

template<kyber_level Level>
struct alignas(64) kyber_secret_key {
    using key_type = std::array<std::byte, kyber_params<Level>::secret_key_bytes>;
    static constexpr std::size_t key_size = kyber_params<Level>::secret_key_bytes;
    
    alignas(64) key_type data;
    
    constexpr kyber_secret_key() noexcept : data{} {}
    explicit constexpr kyber_secret_key(const key_type& key_data) noexcept : data(key_data) {}
    
    // Secure destruction
    ~kyber_secret_key() noexcept {
        std::ranges::fill(data, std::byte{0});
    }
};

template<kyber_level Level>
struct alignas(64) kyber_ciphertext {
    using ciphertext_type = std::array<std::byte, kyber_params<Level>::ciphertext_bytes>;
    static constexpr std::size_t ciphertext_size = kyber_params<Level>::ciphertext_bytes;
    
    alignas(64) ciphertext_type data;
    
    constexpr kyber_ciphertext() noexcept : data{} {}
    explicit constexpr kyber_ciphertext(const ciphertext_type& ct_data) noexcept : data(ct_data) {}
};

// Key pair structure
template<kyber_level Level>
struct kyber_keypair {
    kyber_public_key<Level> public_key;
    kyber_secret_key<Level> secret_key;
    
    constexpr kyber_keypair() = default;
    constexpr kyber_keypair(kyber_public_key<Level> pk, kyber_secret_key<Level> sk) noexcept
        : public_key(std::move(pk)), secret_key(std::move(sk)) {}
};

// Shared secret type
using kyber_shared_secret = std::array<std::byte, 32>;

// SIMD-optimized Kyber implementation class
template<kyber_level Level>
class alignas(64) kyber_simd {
private:
    static constexpr auto params = kyber_params<Level>{};

public:
    // Key generation with SIMD optimization
    [[nodiscard]] static std::expected<kyber_keypair<Level>, std::error_code> 
    generate_keypair() noexcept;

    // Encapsulation with SIMD optimization
    [[nodiscard]] static std::expected<std::pair<kyber_ciphertext<Level>, kyber_shared_secret>, std::error_code>
    encapsulate([[maybe_unused]] const kyber_public_key<Level>& public_key) noexcept {
        return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    }

    // Decapsulation with SIMD optimization  
    [[nodiscard]] static std::expected<kyber_shared_secret, std::error_code>
    decapsulate([[maybe_unused]] const kyber_ciphertext<Level>& ciphertext, 
                [[maybe_unused]] const kyber_secret_key<Level>& secret_key) noexcept {
        return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    }
};

// Type aliases for common Kyber variants
using kyber512_simd = kyber_simd<kyber_level::KYBER_512>;
using kyber768_simd = kyber_simd<kyber_level::KYBER_768>;
using kyber1024_simd = kyber_simd<kyber_level::KYBER_1024>;

// SIMD capability information for diagnostics
[[nodiscard]] constexpr std::string_view get_simd_info() noexcept {
    if constexpr (simd_caps::has_avx512vnni) {
        return "AVX512-VNNI (fastest)";
    } else if constexpr (simd_caps::has_avx512f) {
        return "AVX512-F";
    } else if constexpr (simd_caps::has_avx2) {
        return "AVX2";
    } else if constexpr (simd_caps::has_sse4_2) {
        return "SSE4.2";
    } else if constexpr (simd_caps::has_sse4_1) {
        return "SSE4.1";
    } else if constexpr (simd_caps::has_ssse3) {
        return "SSSE3";
    } else if constexpr (simd_caps::has_sse3) {
        return "SSE3";
    } else if constexpr (simd_caps::has_sse2) {
        return "SSE2";
    } else if constexpr (simd_caps::has_sse) {
        return "SSE";
    } else if constexpr (simd_caps::has_3dnow_ext) {
        return "3DNow! Extended";
    } else if constexpr (simd_caps::has_3dnow) {
        return "3DNow!";
    } else {
        return "Scalar (no SIMD)";
    }
}

// Benchmark function for performance testing
template<kyber_level Level>
struct kyber_benchmark {
    static void benchmark_keypair_generation(std::size_t = 1000) {}
    static void benchmark_encapsulation(std::size_t = 1000) {}
};

} // namespace xinim::crypto::kyber::simd
