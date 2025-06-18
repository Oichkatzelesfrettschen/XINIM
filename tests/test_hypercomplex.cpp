#include "../kernel/fano_octonion.hpp"
#include "../kernel/quaternion_spinlock.hpp"
#include "../kernel/sedenion.hpp"
#include <array>
#include <cassert>
#include <thread>

int main() {
    using hyper::Quaternion;
    using hyper::QuaternionSpinlock;
    QuaternionSpinlock lock;
    Quaternion ticket{0.0F, 1.0F, 0.0F, 0.0F};
    lock.lock(ticket);
    lock.unlock(ticket);

    lattice::Octonion a{};
    a.comp[1] = 1;
    lattice::Octonion b{};
    b.comp[2] = 1;
    auto prod = lattice::fano_multiply(a, b);
    assert(prod.comp[3] == 1);

    auto pair = hyper::zpair_generate();
    std::array<uint8_t, 16> msg{};
    msg[0] = 42;
    auto cipher = hyper::zlock_encrypt(pair.pub, msg);
    auto plain = hyper::zlock_decrypt(pair.pub, cipher);
    assert(plain[0] == 42);
    return 0;
}
