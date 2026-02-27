/**
 * @file test_capability_mutex.cpp
 * @brief Unit tests for CapabilityMutex and LockManager.
 */

#include "capability_mutex.hpp"
#include "lock_manager.hpp"
#include <cassert>
#include <atomic>

using namespace xinim::sync;

static std::atomic<xinim::pid_t> next_pid{1};

static xinim::pid_t get_test_pid() {
    return next_pid.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Create a token with non-zero octonion proof (passes verify_token).
 *
 * The actual verify_token checks that at least one octonion component is
 * non-zero, so we set all components to non-zero values.
 */
static CapabilityToken create_token(xinim::pid_t pid) {
    return CapabilityToken{
        .token_id = static_cast<uint64_t>(pid) << 32 | 0xDEADBEEFu,
        .issuer_pid = pid,
        .expiry_time = 0,
        .rights = CapabilityToken::RIGHT_READ | CapabilityToken::RIGHT_WRITE,
        .proof = lattice::Octonion{{1, 2, 3, 4, 5, 6, 7, 8}},
    };
}

/**
 * @brief Test basic lock/unlock with capabilities.
 */
static void test_basic_lock() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    assert(!mutex.is_locked());

    // Lock with valid capability
    assert(mutex.lock(pid, token));
    assert(mutex.is_locked());
    assert(mutex.owner() == pid);

    // Unlock
    mutex.unlock(pid);
    assert(!mutex.is_locked());
}

/**
 * @brief Token with all-zero octonion proof should fail verify_token.
 */
static void test_zero_proof_rejected() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();

    CapabilityToken bad_token{
        .token_id = 42,
        .issuer_pid = pid,
        .expiry_time = 0,
        .rights = CapabilityToken::RIGHT_READ | CapabilityToken::RIGHT_WRITE,
        .proof = lattice::Octonion{}, // all zeros
    };

    assert(!mutex.lock(pid, bad_token));
    assert(!mutex.is_locked());
}

/**
 * @brief Unlock by wrong PID is a no-op.
 */
static void test_wrong_pid_unlock() {
    CapabilityMutex mutex;
    xinim::pid_t owner = get_test_pid();
    xinim::pid_t other = get_test_pid();
    CapabilityToken token = create_token(owner);

    mutex.lock(owner, token);
    assert(mutex.is_locked());

    // Wrong PID cannot unlock
    mutex.unlock(other);
    assert(mutex.is_locked());
    assert(mutex.owner() == owner);

    // Correct PID can unlock
    mutex.unlock(owner);
    assert(!mutex.is_locked());
}

/**
 * @brief Force unlock releases regardless of owner.
 */
static void test_force_unlock() {
    CapabilityMutex mutex;
    xinim::pid_t pid = get_test_pid();
    CapabilityToken token = create_token(pid);

    mutex.lock(pid, token);
    assert(mutex.is_locked());

    mutex.force_unlock();
    assert(!mutex.is_locked());
}

/**
 * @brief Lock manager tracks registered locks.
 */
static void test_lock_manager() {
    LockManager mgr;
    CapabilityMutex m1, m2;
    xinim::pid_t pid = get_test_pid();

    mgr.register_lock(pid, &m1);
    mgr.register_lock(pid, &m2);

    mgr.unregister_lock(pid, &m1);

    // release_all_for_pid should call force_unlock on remaining
    // (This tests the tracking, not the force_unlock itself)
    mgr.release_all_for_pid(pid);
}

int main() {
    test_basic_lock();
    test_zero_proof_rejected();
    test_wrong_pid_unlock();
    test_force_unlock();
    test_lock_manager();

    return 0;
}
