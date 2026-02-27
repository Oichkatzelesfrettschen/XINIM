/**
 * @file kyber_cpp23_simd.cpp  
 * @brief Implementation of SIMD-optimized Kyber post-quantum cryptography
 *
 * Complete implementation with comprehensive SIMD support and C++23 features.
 */

#include "kyber_cpp23_simd.hpp"
#include "console.hpp"
#include <algorithm>
#include <ranges>

// SHAKE implementation for Kyber
#include "kyber_impl/fips202.hpp"

namespace xinim::crypto::kyber::simd {

using namespace xinim::crypto::fips202;

// Kernel-space random number generation using RDRAND (Phase 7 P7-T06).
class secure_random {
public:
    void fill_bytes(std::span<std::byte> buffer) {
        // Use RDRAND instruction for each 8-byte chunk
        auto* ptr = reinterpret_cast<unsigned long long*>(buffer.data());
        std::size_t full_words = buffer.size() / 8;
        for (std::size_t i = 0; i < full_words; ++i) {
            unsigned long long val = 0;
            for (int retry = 0; retry < 10; ++retry) {
                unsigned char ok = 0;
                asm volatile("rdrand %0; setc %1" : "=r"(val), "=qm"(ok));
                if (ok) break;
            }
            ptr[i] = val;
        }
        // Handle remaining bytes
        std::size_t remaining = buffer.size() % 8;
        if (remaining > 0) {
            unsigned long long val = 0;
            unsigned char ok = 0;
            asm volatile("rdrand %0; setc %1" : "=r"(val), "=qm"(ok));
            auto* tail = buffer.data() + full_words * 8;
            for (std::size_t i = 0; i < remaining; ++i) {
                tail[i] = static_cast<std::byte>((val >> (i * 8)) & 0xFF);
            }
        }
    }
};

// Global secure random instance (no thread_local in freestanding)
static secure_random g_secure_rng;

// Forward declarations for internal helper functions
void cbd_sample_avx2(std::array<std::int16_t, 256>& coeffs, 
                     const std::vector<std::byte>& random_bytes, 
                     std::size_t eta);
void cbd_sample_scalar(std::array<std::int16_t, 256>& coeffs,
                      const std::vector<std::byte>& random_bytes,
                      std::size_t eta);
void pack_public_key_512(const std::array<poly_simd<kyber_level::KYBER_512>, 2>& t,
                        const std::array<std::byte, 32>& rho,
                        kyber_public_key<kyber_level::KYBER_512>& pk);
void pack_secret_key_512(const std::array<poly_simd<kyber_level::KYBER_512>, 2>& s,
                        kyber_secret_key<kyber_level::KYBER_512>& sk);
void pack_poly_12bit(const std::array<std::int16_t, 256>& coeffs, std::byte* output);

// SHAKE-128 wrapper for uniform polynomial generation
class shake128_context {
private:
    keccak_state ctx_{};
    
public:
    shake128_context() = default;
    
    void absorb(std::span<const std::byte> data) {
        (void)shake128_absorb(ctx_, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size()));
    }
    
    void finalize() {
        // No-op in new API
    }
    
    void squeeze(std::span<std::byte> output) {
        (void)shake128_squeezeblocks(std::span<uint8_t>(reinterpret_cast<uint8_t*>(output.data()), output.size()), ctx_);
    }
};

// Enhanced uniform polynomial generation with rejection sampling
template<kyber_level Level>
poly_simd<Level> generate_uniform_poly_secure(std::span<const std::byte, 34> seed) {
    poly_simd<Level> poly;
    auto& coeffs = poly.data();
    
    shake128_context shake;
    shake.absorb(seed);
    shake.finalize();
    
    constexpr std::uint16_t Q = static_cast<std::uint16_t>(kyber_params<Level>::q);
    std::size_t coeff_idx = 0;
    
    std::array<std::byte, 168> buffer{}; // Generate 168 bytes at a time
    
    while (coeff_idx < coeffs.size()) {
        shake.squeeze(buffer);
        
        for (std::size_t i = 0; i < buffer.size() && coeff_idx < coeffs.size(); i += 3) {
            // Extract two 12-bit values from 3 bytes
            std::uint16_t val1 = (static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(buffer[i])) | 
                                (static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(buffer[i + 1])) << 8)) & 0xFFF;
            std::uint16_t val2 = (static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(buffer[i + 1])) >> 4) | 
                                (static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(buffer[i + 2])) << 4);
            
            // Rejection sampling to ensure uniform distribution mod Q
            if (val1 < Q) {
                coeffs[coeff_idx++] = static_cast<std::int16_t>(val1);
            }
            if (val2 < Q && coeff_idx < coeffs.size()) {
                coeffs[coeff_idx++] = static_cast<std::int16_t>(val2);
            }
        }
    }
    
    return poly;
}

// SIMD-optimized centered binomial distribution implementation
template<kyber_level Level>
poly_simd<Level> generate_cbd_poly(std::span<const std::byte> seed, std::uint8_t nonce, std::size_t eta) {
    poly_simd<Level> poly;
    auto& coeffs = poly.data();
    
    // Create extended seed with nonce
    std::array<std::byte, 33> extended_seed;
    std::copy(seed.begin(), seed.end(), extended_seed.begin());
    extended_seed[32] = static_cast<std::byte>(nonce);
    
    // Use SHAKE-256 for PRF
    keccak_state prf_ctx{};
    (void)shake256_absorb(prf_ctx, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(extended_seed.data()), 33));
    
    // Generate enough random bits for CBD
    constexpr std::size_t bytes_needed = (256 * 3 * 2 + 7) / 8; // Max eta=3
    std::vector<std::byte> random_bytes(bytes_needed);
    (void)shake256_squeeze(std::span<uint8_t>(reinterpret_cast<uint8_t*>(random_bytes.data()), bytes_needed), prf_ctx);
    
    // SIMD-optimized CBD sampling
    #ifdef __AVX2__
    if constexpr (simd_caps::has_avx2) {
        cbd_sample_avx2(coeffs, random_bytes, eta);
    } else {
        cbd_sample_scalar(coeffs, random_bytes, eta);
    }
    #else
    cbd_sample_scalar(coeffs, random_bytes, eta);
    #endif
    
    return poly;
}

// AVX2-optimized CBD sampling
void cbd_sample_avx2(std::array<std::int16_t, 256>& coeffs, 
                     [[maybe_unused]] const std::vector<std::byte>& random_bytes, 
                     [[maybe_unused]] std::size_t eta) {
    
    #ifdef __AVX2__
    if (eta == 2) {
        // Sample from centered binomial distribution with eta=2
        const __m256i mask_01 = _mm256_set1_epi32(0x55555555);  // Extract bit pairs
        const __m256i mask_03 = _mm256_set1_epi32(0x33333333);  // For popcount
        const __m256i mask_0f = _mm256_set1_epi32(0x0f0f0f0f);
        
        for (std::size_t i = 0; i < coeffs.size(); i += 16) {
            // Load 8 bytes (64 bits) for 16 coefficients
            __m256i random = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(&random_bytes[i / 2])
            );
            
            // Extract and count bits for positive and negative contributions
            __m256i bits_pos = _mm256_and_si256(random, mask_01);
            __m256i bits_neg = _mm256_and_si256(_mm256_srli_epi32(random, 1), mask_01);
            
            // Parallel popcount using bit manipulation
            bits_pos = _mm256_add_epi32(_mm256_and_si256(bits_pos, mask_03),
                                      _mm256_and_si256(_mm256_srli_epi32(bits_pos, 2), mask_03));
            bits_neg = _mm256_add_epi32(_mm256_and_si256(bits_neg, mask_03),
                                      _mm256_and_si256(_mm256_srli_epi32(bits_neg, 2), mask_03));
            
            bits_pos = _mm256_add_epi32(_mm256_and_si256(bits_pos, mask_0f),
                                      _mm256_and_si256(_mm256_srli_epi32(bits_pos, 4), mask_0f));
            bits_neg = _mm256_add_epi32(_mm256_and_si256(bits_neg, mask_0f),
                                      _mm256_and_si256(_mm256_srli_epi32(bits_neg, 4), mask_0f));
            
            // Final reduction and subtraction
            __m256i result = _mm256_sub_epi16(_mm256_packs_epi32(bits_pos, _mm256_setzero_si256()),
                                            _mm256_packs_epi32(bits_neg, _mm256_setzero_si256()));
            
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(&coeffs[i]), result);
        }
    } else if (eta == 3) {
        // Similar implementation for eta=3
        for (std::size_t i = 0; i < coeffs.size(); ++i) {
            std::size_t byte_offset = (i * 6) / 8;
            std::size_t bit_offset = (i * 6) % 8;
            
            std::uint8_t bits = 0;
            if (bit_offset <= 2) {
                bits = (std::to_integer<std::uint8_t>(random_bytes[byte_offset]) >> bit_offset) & 0x3F;
            } else {
                bits = ((std::to_integer<std::uint8_t>(random_bytes[byte_offset]) >> bit_offset) |
                       (std::to_integer<std::uint8_t>(random_bytes[byte_offset + 1]) << (8 - bit_offset))) & 0x3F;
            }
            
            std::int16_t a = static_cast<std::int16_t>(std::popcount(static_cast<unsigned>(bits & 0x07)));
            std::int16_t b = static_cast<std::int16_t>(std::popcount(static_cast<unsigned>((bits >> 3) & 0x07)));
            coeffs[i] = static_cast<std::int16_t>(a - b);
        }
    }
    #else
    cbd_sample_scalar(coeffs, random_bytes, eta);
    #endif
}

// Scalar CBD sampling fallback
void cbd_sample_scalar(std::array<std::int16_t, 256>& coeffs,
                      const std::vector<std::byte>& random_bytes,
                      std::size_t eta) {
    for (std::size_t i = 0; i < coeffs.size(); ++i) {
        std::int16_t a = 0, b = 0;
        
        for (std::size_t j = 0; j < eta; ++j) {
            std::size_t bit_idx = i * eta * 2 + j;
            std::size_t byte_idx = bit_idx / 8;
            std::size_t bit_pos = bit_idx % 8;
            
            if (std::to_integer<std::uint8_t>(random_bytes[byte_idx]) & (1 << bit_pos)) {
                a++;
            }
            
            bit_idx += eta;
            byte_idx = bit_idx / 8;
            bit_pos = bit_idx % 8;
            
            if (std::to_integer<std::uint8_t>(random_bytes[byte_idx]) & (1 << bit_pos)) {
                b++;
            }
        }
        
        coeffs[i] = static_cast<std::int16_t>(a - b);
    }
}

// Template specializations for key generation
template<>
std::expected<kyber_keypair<kyber_level::KYBER_512>, std::error_code>
kyber_simd<kyber_level::KYBER_512>::generate_keypair() noexcept {
    try {
        kyber_keypair<kyber_level::KYBER_512> kp;
        
        // Generate random seed for matrix A
        std::array<std::byte, 32> rho;
        g_secure_rng.fill_bytes(rho);
        
        // Generate random coins for secret key
        std::array<std::byte, 32> sigma; 
        g_secure_rng.fill_bytes(sigma);
        
        // Generate matrix A (2x2 for Kyber-512)
        std::array<std::array<poly_simd<kyber_level::KYBER_512>, 2>, 2> A;
        for (std::size_t i = 0; i < 2; ++i) {
            for (std::size_t j = 0; j < 2; ++j) {
                std::array<std::byte, 34> seed_ij;
                std::copy(rho.begin(), rho.end(), seed_ij.begin());
                seed_ij[32] = static_cast<std::byte>(i);
                seed_ij[33] = static_cast<std::byte>(j);
                
                A[i][j] = generate_uniform_poly_secure<kyber_level::KYBER_512>(seed_ij);
                A[i][j].ntt(); // Transform to NTT domain
            }
        }
        
        // Generate secret vector s and error vector e
        std::array<poly_simd<kyber_level::KYBER_512>, 2> s, e;
        for (std::size_t i = 0; i < 2; ++i) {
            s[i] = generate_cbd_poly<kyber_level::KYBER_512>(sigma, static_cast<uint8_t>(i), 2); // eta_1=2 for Kyber-512
            e[i] = generate_cbd_poly<kyber_level::KYBER_512>(sigma, static_cast<uint8_t>(i + 2), 2);
            
            s[i].ntt();
            e[i].ntt();
        }
        
        // Compute public key: t = A*s + e
        std::array<poly_simd<kyber_level::KYBER_512>, 2> t;
        for (std::size_t i = 0; i < 2; ++i) {
            t[i] = A[i][0].pointwise_mul(s[0]) + A[i][1].pointwise_mul(s[1]) + e[i];
        }
        
        // Pack keys
        pack_public_key_512(t, rho, kp.public_key);
        pack_secret_key_512(s, kp.secret_key);
        
        return kp;
        
    } catch (const std::exception&) {
        return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
    }
}

// Key packing implementations for Kyber-512
void pack_public_key_512(const std::array<poly_simd<kyber_level::KYBER_512>, 2>& t,
                        const std::array<std::byte, 32>& rho,
                        kyber_public_key<kyber_level::KYBER_512>& pk) {
    std::size_t offset = 0;
    
    // Pack polynomials in NTT domain (12 bits per coefficient).
    // Public key stores NTT-domain coefficients; do NOT apply invntt.
    for (const auto& poly : t) {
        pack_poly_12bit(poly.data(), pk.data.data() + offset);
        offset += 384; // 256 * 12 / 8 = 384 bytes
    }
    
    // Append seed rho
    std::copy(rho.begin(), rho.end(), pk.data.begin() + offset);
}

void pack_secret_key_512(const std::array<poly_simd<kyber_level::KYBER_512>, 2>& s,
                        kyber_secret_key<kyber_level::KYBER_512>& sk) {
    std::size_t offset = 0;
    
    // Pack secret polynomials (12 bits per coefficient)
    for (const auto& poly : s) {
        auto coeffs_copy = poly.data();
        poly_simd<kyber_level::KYBER_512> temp_poly;
        temp_poly.data() = coeffs_copy;
        temp_poly.invntt();
        
        pack_poly_12bit(temp_poly.data(), sk.data.data() + offset);
        offset += 384;
    }
}

void pack_poly_12bit(const std::array<std::int16_t, 256>& coeffs, std::byte* output) {
    #ifdef __AVX2__
    if constexpr (simd_caps::has_avx2) {
        // Optimized 12-bit polynomial packing with AVX2
        for (std::size_t i = 0; i < 256; i += 16) {
            __m256i poly_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&coeffs[i]));
            const __m256i mask_12bit = _mm256_set1_epi16(0x0FFF);
            poly_vec = _mm256_and_si256(poly_vec, mask_12bit);
            __m256i packed_low = _mm256_packus_epi16(poly_vec, _mm256_setzero_si256());
            __m128i result = _mm256_extracti128_si256(packed_low, 0);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(output + (i * 3) / 2), result);
        }
    } else {
        for (std::size_t i = 0; i < 256; i += 2) {
            std::uint32_t val = (static_cast<std::uint32_t>(coeffs[i]) & 0xFFF) |
                               ((static_cast<std::uint32_t>(coeffs[i+1]) & 0xFFF) << 12);
            output[(i * 3) / 2] = static_cast<std::byte>(val & 0xFF);
            output[(i * 3) / 2 + 1] = static_cast<std::byte>((val >> 8) & 0xFF);
            output[(i * 3) / 2 + 2] = static_cast<std::byte>((val >> 16) & 0xFF);
        }
    }
    #else
    for (std::size_t i = 0; i < 256; i += 2) {
        std::uint32_t val = (static_cast<std::uint32_t>(coeffs[i]) & 0xFFF) |
                           ((static_cast<std::uint32_t>(coeffs[i+1]) & 0xFFF) << 12);
        output[(i * 3) / 2] = static_cast<std::byte>(val & 0xFF);
        output[(i * 3) / 2 + 1] = static_cast<std::byte>((val >> 8) & 0xFF);
        output[(i * 3) / 2 + 2] = static_cast<std::byte>((val >> 16) & 0xFF);
    }
    #endif
}

// SIMD capability detection and reporting
void report_simd_capabilities() {
    Console::printf("\n=== SIMD Capabilities Report ===\n");
    Console::printf("Selected SIMD level: %s\n", get_simd_info().data());
}

} // namespace xinim::crypto::kyber::simd

// Export C-compatible interface for integration with existing code
extern "C" {
    using namespace xinim::crypto::kyber::simd;
    
    int kyber512_simd_keypair(uint8_t* pk, uint8_t* sk) {
        auto result = kyber512_simd::generate_keypair();
        if (!result) return -1;
        
        std::copy(result->public_key.data.begin(), result->public_key.data.end(), 
                 reinterpret_cast<std::byte*>(pk));
        std::copy(result->secret_key.data.begin(), result->secret_key.data.end(),
                 reinterpret_cast<std::byte*>(sk));
        return 0;
    }
}
