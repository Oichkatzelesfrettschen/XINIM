// Random bytes generator - C++23 with architecture-specific optimizations
#include "randombytes.hpp"
#include <xinim/hal/arch.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <random>
#include <span>

#ifdef __linux__
    #include <sys/random.h>
#elif defined(__APPLE__)
    #include <Security/SecRandom.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <wincrypt.h>
#endif

namespace xinim::crypto {

// Thread-local random engine for performance
thread_local std::random_device rd;
thread_local std::mt19937_64 rng(rd());
thread_local std::uniform_int_distribution<uint8_t> dist(0, 255);

// Architecture-optimized random generation
class RandomBytesGenerator {
public:
    // Main random bytes function with OS-specific secure implementations
    static void generate(std::span<uint8_t> buffer) noexcept {
#ifdef __linux__
        generate_linux(buffer);
#elif defined(__APPLE__)
        generate_macos(buffer);
#elif defined(_WIN32)
        generate_windows(buffer);
#else
        generate_fallback(buffer);
#endif
    }
    
    // Test-only pseudo-random (deterministic for testing)
    static void generate_deterministic(std::span<uint8_t> buffer, uint64_t seed = 0) noexcept {
        std::mt19937_64 test_rng(seed);
        std::uniform_int_distribution<uint8_t> test_dist(0, 255);
        
        // Use SIMD for faster generation where possible
        if constexpr (xinim::hal::is_x86_64) {
            generate_simd_x86(buffer, test_rng);
        } else if constexpr (xinim::hal::is_arm64) {
            generate_simd_arm(buffer, test_rng);
        } else {
            std::generate(buffer.begin(), buffer.end(), 
                         [&test_rng, &test_dist]() { return test_dist(test_rng); });
        }
    }

private:
#ifdef __linux__
    static void generate_linux(std::span<uint8_t> buffer) noexcept {
        size_t offset = 0;
        while (offset < buffer.size()) {
            ssize_t ret = getrandom(buffer.data() + offset, 
                                   buffer.size() - offset, 0);
            if (ret > 0) {
                offset += ret;
            } else if (ret == -1 && errno == EINTR) {
                continue; // Retry on interrupt
            } else {
                // Fallback to pseudo-random
                generate_fallback(buffer.subspan(offset));
                break;
            }
        }
    }
#endif

#ifdef __APPLE__
    static void generate_macos(std::span<uint8_t> buffer) noexcept {
        if (SecRandomCopyBytes(kSecRandomDefault, buffer.size(), 
                              buffer.data()) != errSecSuccess) {
            // Fallback to pseudo-random
            generate_fallback(buffer);
        }
    }
#endif

#ifdef _WIN32
    static void generate_windows(std::span<uint8_t> buffer) noexcept {
        HCRYPTPROV hProvider = 0;
        if (CryptAcquireContext(&hProvider, nullptr, nullptr, 
                               PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            if (!CryptGenRandom(hProvider, buffer.size(), buffer.data())) {
                generate_fallback(buffer);
            }
            CryptReleaseContext(hProvider, 0);
        } else {
            generate_fallback(buffer);
        }
    }
#endif

    // Fallback using C++ random
    static void generate_fallback(std::span<uint8_t> buffer) noexcept {
        std::generate(buffer.begin(), buffer.end(), 
                     []() { return dist(rng); });
    }
    
#ifdef XINIM_ARCH_X86_64
    static void generate_simd_x86(std::span<uint8_t> buffer, 
                                  std::mt19937_64& test_rng) noexcept {
        // Generate 32 bytes at a time using AVX2
        size_t i = 0;
        if (__builtin_cpu_supports("avx2") && buffer.size() >= 32) {
            alignas(32) std::array<uint64_t, 4> rand_data;
            
            while (i + 32 <= buffer.size()) {
                // Generate 4 random 64-bit values
                for (auto& val : rand_data) {
                    val = test_rng();
                }
                
                // Copy to output buffer
                std::memcpy(&buffer[i], rand_data.data(), 32);
                i += 32;
            }
        }
        
        // Handle remainder
        std::uniform_int_distribution<uint8_t> test_dist(0, 255);
        for (; i < buffer.size(); ++i) {
            buffer[i] = test_dist(test_rng);
        }
    }
#endif

#ifdef XINIM_ARCH_ARM64
    static void generate_simd_arm(std::span<uint8_t> buffer,
                                  std::mt19937_64& test_rng) noexcept {
        // Generate 16 bytes at a time using NEON
        size_t i = 0;
        if (buffer.size() >= 16) {
            alignas(16) std::array<uint64_t, 2> rand_data;
            
            while (i + 16 <= buffer.size()) {
                // Generate 2 random 64-bit values
                rand_data[0] = test_rng();
                rand_data[1] = test_rng();
                
                // Copy to output buffer
                std::memcpy(&buffer[i], rand_data.data(), 16);
                i += 16;
            }
        }
        
        // Handle remainder
        std::uniform_int_distribution<uint8_t> test_dist(0, 255);
        for (; i < buffer.size(); ++i) {
            buffer[i] = test_dist(test_rng);
        }
    }
#endif
};

} // namespace xinim::crypto

// C-compatible interface
extern "C" {

void randombytes(uint8_t* out, size_t outlen) {
    xinim::crypto::RandomBytesGenerator::generate({out, outlen});
}

// Deterministic version for testing
void randombytes_deterministic(uint8_t* out, size_t outlen, uint64_t seed) {
    xinim::crypto::RandomBytesGenerator::generate_deterministic({out, outlen}, seed);
}

}