#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp"
#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion_spinlock.hpp"
#include <vector>
#include <thread>
#include <iostream> // For potential debug prints, not for assertions
#include <cassert>  // Using assert for tests for simplicity in this context
#include <cmath>    // For std::fabs
#include <limits>   // For std::numeric_limits

// Helper for comparing doubles with tolerance
bool approx_equal(double a, double b, double epsilon = XINIM::Algebraic::QUATERNION_EPSILON * 10) {
    return std::fabs(a - b) < epsilon;
}

// Helper for comparing quaternions with tolerance
bool approx_equal_quat(const XINIM::Algebraic::Quaternion& q1, const XINIM::Algebraic::Quaternion& q2, double epsilon = XINIM::Algebraic::QUATERNION_EPSILON * 10) {
    return approx_equal(q1.w, q2.w, epsilon) &&
           approx_equal(q1.x, q2.x, epsilon) &&
           approx_equal(q1.y, q2.y, epsilon) &&
           approx_equal(q1.z, q2.z, epsilon);
}


void test_quaternion_constructors_and_accessors() {
    std::cout << "Running test_quaternion_constructors_and_accessors...\n";
    XINIM::Algebraic::Quaternion q1; // Default constructor
    assert(q1.w == 0.0 && q1.x == 0.0 && q1.y == 0.0 && q1.z == 0.0);
    assert(q1.is_zero());

    XINIM::Algebraic::Quaternion q2(1.0, 2.0, 3.0, 4.0);
    assert(q2.w == 1.0 && q2.x == 2.0 && q2.y == 3.0 && q2.z == 4.0);

    std::array<double, 3> vec = {2.0, 3.0, 4.0};
    XINIM::Algebraic::Quaternion q3(1.0, vec);
    assert(q3.w == 1.0 && q3.x == 2.0 && q3.y == 3.0 && q3.z == 4.0);

    assert(XINIM::Algebraic::Quaternion::identity().w == 1.0 && XINIM::Algebraic::Quaternion::identity().is_unit(1e-9));
    assert(XINIM::Algebraic::Quaternion::zero().is_zero());
    std::cout << "test_quaternion_constructors_and_accessors PASSED.\n";
}

void test_quaternion_arithmetic() {
    std::cout << "Running test_quaternion_arithmetic...\n";
    XINIM::Algebraic::Quaternion q1(1, 2, 3, 4);
    XINIM::Algebraic::Quaternion q2(5, 6, 7, 8);

    // Addition
    XINIM::Algebraic::Quaternion q_add = q1 + q2;
    assert(approx_equal_quat(q_add, XINIM::Algebraic::Quaternion(6, 8, 10, 12)));

    // Subtraction
    XINIM::Algebraic::Quaternion q_sub = q1 - q2;
    assert(approx_equal_quat(q_sub, XINIM::Algebraic::Quaternion(-4, -4, -4, -4)));

    // Scalar Multiplication
    XINIM::Algebraic::Quaternion q_mul_scalar = q1 * 2.0;
    assert(approx_equal_quat(q_mul_scalar, XINIM::Algebraic::Quaternion(2, 4, 6, 8)));
    XINIM::Algebraic::Quaternion q_mul_scalar2 = 2.0 * q1;
    assert(approx_equal_quat(q_mul_scalar2, XINIM::Algebraic::Quaternion(2, 4, 6, 8)));


    // Scalar Division
    XINIM::Algebraic::Quaternion q_div_scalar = q1 / 2.0;
    assert(approx_equal_quat(q_div_scalar, XINIM::Algebraic::Quaternion(0.5, 1.0, 1.5, 2.0)));

    XINIM::Algebraic::Quaternion q_div_zero = q1 / 0.0;
    assert(q_div_zero.is_zero()); // Should return zero for division by zero

    // Hamilton Product: q1 * q2
    // (1,2,3,4) * (5,6,7,8) = (w1w2 - x1x2 - y1y2 - z1z2,  // w
    //                          w1x2 + x1w2 + y1z2 - z1y2,  // x
    //                          w1y2 - x1z2 + y1w2 + z1x2,  // y
    //                          w1z2 + x1y2 - y1x2 + z1w2)  // z
    // w = 1*5 - 2*6 - 3*7 - 4*8 = 5 - 12 - 21 - 32 = -60
    // x = 1*6 + 2*5 + 3*8 - 4*7 = 6 + 10 + 24 - 28 = 12
    // y = 1*7 - 2*8 + 3*5 + 4*6 = 7 - 16 + 15 + 24 = 30
    // z = 1*8 + 2*7 - 3*6 + 4*5 = 8 + 14 - 18 + 20 = 24
    XINIM::Algebraic::Quaternion q_hamilton = q1 * q2;
    assert(approx_equal_quat(q_hamilton, XINIM::Algebraic::Quaternion(-60, 12, 30, 24)));
    std::cout << "test_quaternion_arithmetic PASSED.\n";
}

void test_quaternion_operations() {
    std::cout << "Running test_quaternion_operations...\n";
    XINIM::Algebraic::Quaternion q(1, 2, 3, 4);

    // Conjugate
    XINIM::Algebraic::Quaternion q_conj = q.conjugate();
    assert(approx_equal_quat(q_conj, XINIM::Algebraic::Quaternion(1, -2, -3, -4)));

    // Norm_sq: 1*1 + 2*2 + 3*3 + 4*4 = 1 + 4 + 9 + 16 = 30
    assert(approx_equal(q.norm_sq(), 30.0));

    // Norm
    assert(approx_equal(q.norm(), std::sqrt(30.0)));

    // Inverse: conjugate() / norm_sq()
    XINIM::Algebraic::Quaternion q_inv = q.inverse();
    XINIM::Algebraic::Quaternion expected_inv(1.0/30.0, -2.0/30.0, -3.0/30.0, -4.0/30.0);
    assert(approx_equal_quat(q_inv, expected_inv));

    // q * q.inverse() should be identity
    XINIM::Algebraic::Quaternion q_identity = q * q_inv;
    assert(approx_equal_quat(q_identity, XINIM::Algebraic::Quaternion::identity()));

    // Inverse of zero quaternion
    XINIM::Algebraic::Quaternion q_zero = XINIM::Algebraic::Quaternion::zero();
    assert(q_zero.inverse().is_zero());

    // Normalize
    XINIM::Algebraic::Quaternion q_normalized = q.normalized();
    assert(q_normalized.is_unit());
    XINIM::Algebraic::Quaternion temp_q = q;
    temp_q.normalize(); // In-place
    assert(temp_q.is_unit());

    // Normalize zero quaternion
    XINIM::Algebraic::Quaternion q_zero_norm = XINIM::Algebraic::Quaternion::zero();
    q_zero_norm.normalize();
    assert(q_zero_norm.is_zero());


    // is_unit / is_zero
    assert(!q.is_unit());
    assert(!q.is_zero());
    assert(XINIM::Algebraic::Quaternion::identity().is_unit());
    assert(XINIM::Algebraic::Quaternion::zero().is_zero());

    std::cout << "test_quaternion_operations PASSED.\n";
}

// Spinlock tests
hyper::QuaternionSpinlock test_lock;
long long counter = 0; // Shared resource for spinlock test
const int num_threads = 4;
const int iterations_per_thread = 100000;

void spinlock_worker_func(int core_id_mock) {
    // Simple ticket generation for testing - real one might be more complex
    hyper::Quaternion ticket(static_cast<float>(core_id_mock),
                             (core_id_mock % 2 == 0) ? 1.0f : -1.0f,
                             (core_id_mock % 3 == 0) ? 1.0f : -1.0f,
                             (core_id_mock % 4 == 0) ? 1.0f : -1.0f);
    // Normalize ticket to make rotations more predictable if needed, though not strictly necessary for this lock design
    // For this test, raw ticket is fine.

    for (int i = 0; i < iterations_per_thread; ++i) {
        hyper::QuaternionLockGuard guard(test_lock, ticket);
        counter++;
    }
}

void test_quaternion_spinlock() {
    std::cout << "Running test_quaternion_spinlock...\n";
    counter = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(spinlock_worker_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    long long expected_counter = static_cast<long long>(num_threads) * iterations_per_thread;
    std::cout << "Counter value: " << counter << ", Expected: " << expected_counter << std::endl;
    assert(counter == expected_counter);
    std::cout << "test_quaternion_spinlock PASSED.\n";
}


int main() {
    std::cout << "Starting Quaternion and Spinlock Tests...\n";

    test_quaternion_constructors_and_accessors();
    test_quaternion_arithmetic();
    test_quaternion_operations();
    test_quaternion_spinlock();

    std::cout << "All Quaternion and Spinlock Tests PASSED.\n";
    return 0;
}
