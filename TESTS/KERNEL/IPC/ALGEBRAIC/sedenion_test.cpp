#include "KERNEL/IPC/ALGEBRAIC/sedenion/sedenion.hpp"
#include "KERNEL/IPC/ALGEBRAIC/octonion/octonion.hpp" // For constructing sedenions
#include <iostream> // For potential debug prints
#include <cassert>  // Using assert for tests
#include <cmath>    // For std::fabs
#include <limits>   // For std::numeric_limits
#include <vector>   // For data for hashing

// Helper for comparing doubles with tolerance from sedenion header's epsilon
bool approx_equal_double_sed(double a, double b, double epsilon = XINIM::Algebraic::SEDENION_EPSILON * 10) {
    return std::fabs(a - b) < epsilon;
}

// Helper for comparing sedenions with tolerance
bool approx_equal_sed(const XINIM::Algebraic::Sedenion& s1, const XINIM::Algebraic::Sedenion& s2, double epsilon = XINIM::Algebraic::SEDENION_EPSILON * 10) {
    for (size_t i = 0; i < 16; ++i) {
        if (!approx_equal_double_sed(s1.c[i], s2.c[i], epsilon)) {
            // std::cout << "Mismatch at index " << i << ": " << s1.c[i] << " vs " << s2.c[i] << std::endl;
            return false;
        }
    }
    return true;
}

void test_sedenion_constructors() {
    std::cout << "Running test_sedenion_constructors...\n";
    XINIM::Algebraic::Sedenion s1; // Default
    assert(s1.is_zero());

    XINIM::Algebraic::Sedenion s2(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
    assert(s2.c[0]==1 && s2.c[15]==16);

    std::array<double, 16> arr = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    XINIM::Algebraic::Sedenion s3(arr);
    assert(approx_equal_sed(s2, s3));

    XINIM::Algebraic::Octonion o1(1,2,3,4,5,6,7,8);
    XINIM::Algebraic::Octonion o2(9,10,11,12,13,14,15,16);
    XINIM::Algebraic::Sedenion s4(o1, o2);
    assert(s4.c[0]==1 && s4.c[7]==8 && s4.c[8]==9 && s4.c[15]==16);

    assert(approx_equal_oct(s4.get_o1(), o1, XINIM::Algebraic::OCTONION_EPSILON * 10));
    assert(approx_equal_oct(s4.get_o2(), o2, XINIM::Algebraic::OCTONION_EPSILON * 10));
    assert(approx_equal_sed(XINIM::Algebraic::Sedenion(s4.get_o1(), s4.get_o2()), s4));

    assert(XINIM::Algebraic::Sedenion::identity().is_unit());
    assert(XINIM::Algebraic::Sedenion::zero().is_zero());
    std::cout << "test_sedenion_constructors PASSED.\n";
}

void test_sedenion_arithmetic() {
    std::cout << "Running test_sedenion_arithmetic...\n";
    XINIM::Algebraic::Sedenion s1(1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8); // Repeated for simplicity
    XINIM::Algebraic::Sedenion s2(8,7,6,5,4,3,2,1,8,7,6,5,4,3,2,1); // Repeated

    // Addition
    XINIM::Algebraic::Sedenion s_add = s1 + s2;
    assert(approx_equal_sed(s_add, XINIM::Algebraic::Sedenion(9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9)));

    // Subtraction
    XINIM::Algebraic::Sedenion s_sub = s1 - s2;
    assert(approx_equal_sed(s_sub, XINIM::Algebraic::Sedenion(-7,-5,-3,-1,1,3,5,7,-7,-5,-3,-1,1,3,5,7)));

    // Scalar Mult/Div
    assert(approx_equal_sed(s1 * 2.0, XINIM::Algebraic::Sedenion(2,4,6,8,10,12,14,16,2,4,6,8,10,12,14,16)));
    assert(approx_equal_sed(s1 / 2.0, XINIM::Algebraic::Sedenion(0.5,1,1.5,2,2.5,3,3.5,4,0.5,1,1.5,2,2.5,3,3.5,4)));
    assert((s1 / 0.0).is_zero()); // Division by zero handled

    // Cayley-Dickson Product (a,b)(c,d) = (ac - d*conj(b), da + conj(c)*b)
    XINIM::Algebraic::Octonion o1a = s1.get_o1();
    XINIM::Algebraic::Octonion o1b = s1.get_o2();
    XINIM::Algebraic::Octonion o2a = s2.get_o1();
    XINIM::Algebraic::Octonion o2b = s2.get_o2();

    XINIM::Algebraic::Octonion res_o1 = (o1a * o2a) - (o2b * o1b.conjugate());
    XINIM::Algebraic::Octonion res_o2 = (o2b * o1a) + (o2a.conjugate() * o1b);
    XINIM::Algebraic::Sedenion expected_prod(res_o1, res_o2);

    XINIM::Algebraic::Sedenion s_mul = s1 * s2;
    assert(approx_equal_sed(s_mul, expected_prod));

    std::cout << "test_sedenion_arithmetic PASSED.\n";
}

void test_sedenion_operations() {
    std::cout << "Running test_sedenion_operations...\n";
    XINIM::Algebraic::Sedenion s(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);

    // Conjugate
    XINIM::Algebraic::Sedenion s_conj = s.conjugate();
    assert(approx_equal_sed(s_conj, XINIM::Algebraic::Sedenion(1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16)));

    // Norm_sq: 1+4+9+16+25+36+49+64 + 81+100+121+144+169+196+225+256 = 204 + 1296 = 1500
    assert(approx_equal_double_sed(s.norm_sq(), 1500.0));
    assert(approx_equal_double_sed(s.norm(), std::sqrt(1500.0)));

    // Inverse (only if norm_sq is not zero)
    XINIM::Algebraic::Sedenion s_inv = s.inverse();
    XINIM::Algebraic::Sedenion expected_inv = s.conjugate() / s.norm_sq();
    assert(approx_equal_sed(s_inv, expected_inv));

    // s * s.inverse() should be identity (with high tolerance due to non-associativity)
    XINIM::Algebraic::Sedenion s_identity = s * s_inv;
    assert(approx_equal_sed(s_identity, XINIM::Algebraic::Sedenion::identity(), XINIM::Algebraic::SEDENION_EPSILON * 1000));

    // Inverse of zero
    assert(XINIM::Algebraic::Sedenion::zero().inverse().is_zero());

    // Normalize
    XINIM::Algebraic::Sedenion s_normalized = s.normalized();
    assert(s_normalized.is_unit(XINIM::Algebraic::SEDENION_EPSILON * 1000));
    XINIM::Algebraic::Sedenion temp_s = s;
    temp_s.normalize();
    assert(temp_s.is_unit(XINIM::Algebraic::SEDENION_EPSILON * 1000));

    // is_unit / is_zero
    assert(!s.is_unit());
    assert(!s.is_zero());
    assert(XINIM::Algebraic::Sedenion::identity().is_unit());
    assert(XINIM::Algebraic::Sedenion::zero().is_zero());

    // Test potential zero divisor: e.g., e1 + e10 (s.c[1]=1, s.c[10]=1)
    // (e_i + e_j) for i != j, i,j != 0. Norm sq is c_i^2 + c_j^2 = 1+1 = 2. Not a zero norm example.
    // Known zero divisors are like (e3+e10)*(e5−e12) = 0.
    // A sedenion s is a zero divisor if it's non-zero and its norm_sq is zero.
    // This is only possible if components are complex, or in algebras over fields other than Reals.
    // For standard sedenions over Reals, norm_sq = sum(c_i^2) is zero iff all c_i are zero.
    // So, for real sedenions, non-zero implies norm_sq > 0. No non-zero s has norm_sq = 0.
    // The concept of "zero divisor" for real sedenions means s != 0, t != 0, s*t = 0.
    // The `is_potential_zero_divisor` checks norm_sq == 0 for non-zero s, which is not possible for real components.
    // Let's test this understanding.
    XINIM::Algebraic::Sedenion s_non_zero(0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0); // e1
    assert(!s_non_zero.is_zero());
    assert(s_non_zero.norm_sq() > 0);
    assert(!s_non_zero.is_potential_zero_divisor()); // Correct, as norm_sq != 0

    std::cout << "test_sedenion_operations PASSED.\n";
}

void test_sedenion_security_placeholders() {
    std::cout << "Running test_sedenion_security_placeholders...\n";
    unsigned char data[] = "This is some test data for hashing.";
    XINIM::Algebraic::Sedenion hashed_s = XINIM::Algebraic::Sedenion::compute_hash_sedenion(data, sizeof(data) -1 );
    assert(!hashed_s.is_zero()); // Placeholder hash should produce non-zero for non-empty data

    XINIM::Algebraic::Sedenion s_orig(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
    XINIM::Algebraic::Sedenion comp_s = s_orig.compute_complementary_sedenion();
    // Basic check, actual properties of complementary are undefined by placeholder
    assert(!comp_s.is_zero() || s_orig.is_zero());

    XINIM::Algebraic::quantum_signature_t signature;
    signature.public_key = s_orig;
    signature.zero_divisor_pair_secret = comp_s;
    // Just checking if the type can be instantiated.
    assert(approx_equal_sed(signature.public_key, s_orig));

    std::cout << "test_sedenion_security_placeholders PASSED.\n";
}

int main() {
    std::cout << "Starting Sedenion Tests...\n";

    test_sedenion_constructors();
    test_sedenion_arithmetic();
    test_sedenion_operations();
    test_sedenion_security_placeholders();

    std::cout << "All Sedenion Tests PASSED.\n";
    return 0;
}
