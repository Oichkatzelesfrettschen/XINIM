#include <xinim/crypto/crypto.hpp>
#include "console.hpp"
#include "kyber_cpp23_simd.hpp"

namespace xinim {
namespace crypto {

bool initialize() {
    Console::printf("Crypto initialization...\n");
    
    // Bootstrap the FIPS 203 (Kyber) post-quantum crypto subsystem
    // Verify SIMD capability at runtime
    std::string_view simd_info = kyber::simd::get_simd_info();
    Console::printf("Kyber SIMD Level: %s\n", simd_info.data());
    
    // Perform a test keypair generation to initialize internal state
    auto test_kp = kyber::simd::kyber_simd<kyber::simd::kyber_level::KYBER_512>::generate_keypair();
    if (!test_kp) {
        Console::printf("CRITICAL: Kyber bootstrap failed during keypair generation.\n");
        return false;
    }
    
    Console::printf("Crypto subsystem bootstrapped successfully.\n");
    return true;
}

} // namespace crypto
} // namespace xinim
