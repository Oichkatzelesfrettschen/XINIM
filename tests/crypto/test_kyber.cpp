#include "kyber.hpp"

#include <cassert>
#include <cstddef>
#include <string_view>
#include <vector>

int main() {
    auto kp = pq::kyber::keypair();

    const std::string_view message = "hello kyber";
    std::vector<std::byte> input(message.size());
    for (size_t i = 0; i < message.size(); ++i) {
        input[i] = std::byte{static_cast<unsigned char>(message[i])};
    }

    auto cipher = pq::kyber::encrypt(input, kp.public_key);
    auto plain = pq::kyber::decrypt(cipher, kp.private_key);

    for (size_t i = 0; i < message.size(); ++i) {
        assert(static_cast<unsigned char>(plain[i]) == static_cast<unsigned char>(input[i]));
    }
    return 0;
}
