#include "pqcrypto.hpp"
#include <random>

namespace pqcrypto {

KeyPair generate_keypair() noexcept {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint16_t> dist(0, 255);

    KeyPair kp{};
    for (auto &b : kp.public_key) {
        b = static_cast<std::uint8_t>(dist(gen));
    }
    for (auto &b : kp.private_key) {
        b = static_cast<std::uint8_t>(dist(gen));
    }
    return kp;
}

std::array<std::uint8_t, 32> establish_secret(const KeyPair &local, const KeyPair &peer) noexcept {
    std::array<std::uint8_t, 32> secret{};
    for (std::size_t i = 0; i < secret.size(); ++i) {
        secret[i] = static_cast<std::uint8_t>(local.private_key[i] ^ peer.public_key[i]);
    }
    return secret;
}

} // namespace pqcrypto
