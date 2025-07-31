#include "KERNEL/IPC/ALGEBRAIC/octonion/octonion.hpp"
#include "KERNEL/IPC/ALGEBRAIC/quaternion/quaternion.hpp" // For constructing octonions
#include <iostream> // For potential debug prints
#include <cassert>  // Using assert for tests
#include <cmath>    // For std::fabs
#include <limits>   // For std::numeric_limits

// Helper for comparing doubles with tolerance from octonion header's epsilon
bool approx_equal_double(double a, double b, double epsilon = XINIM::Algebraic::OCTONION_EPSILON * 10) {
    return std::fabs(a - b) < epsilon;
}

// Helper for comparing octonions with tolerance
bool approx_equal_oct(const XINIM::Algebraic::Octonion& o1, const XINIM::Algebraic::Octonion& o2, double epsilon = XINIM::Algebraic::OCTONION_EPSILON * 10) {
    for (size_t i = 0; i < 8; ++i) {
        if (!approx_equal_double(o1.c[i], o2.c[i], epsilon)) {
            // std::cout << "Mismatch at index " << i << ": " << o1.c[i] << " vs " << o2.c[i] << std::endl;
            return false;
        }
    }
    return true;
}

void test_octonion_constructors() {
    std::cout << "Running test_octonion_constructors...\n";
    XINIM::Algebraic::Octonion o1; // Default
    assert(o1.is_zero());

    XINIM::Algebraic::Octonion o2(1,2,3,4,5,6,7,8);
    assert(o2.c[0]==1 && o2.c[7]==8);

    std::array<double, 8> arr = {1,2,3,4,5,6,7,8};
    XINIM::Algebraic::Octonion o3(arr);
    assert(approx_equal_oct(o2, o3));

    XINIM::Algebraic::Quaternion q1(1,2,3,4);
    XINIM::Algebraic::Quaternion q2(5,6,7,8);
    XINIM::Algebraic::Octonion o4(q1, q2);
    assert(o4.c[0]==1 && o4.c[1]==2 && o4.c[2]==3 && o4.c[3]==4 &&
           o4.c[4]==5 && o4.c[5]==6 && o4.c[6]==7 && o4.c[7]==8);

    assert(approx_equal_oct(o4.get_q1(), q1));
    assert(approx_equal_oct(XINIM::Algebraic::Octonion(o4.get_q1(), o4.get_q2()), o4));


    assert(XINIM::Algebraic::Octonion::identity().is_unit());
    assert(XINIM::Algebraic::Octonion::zero().is_zero());
    std::cout << "test_octonion_constructors PASSED.\n";
}

void test_octonion_arithmetic() {
    std::cout << "Running test_octonion_arithmetic...\n";
    XINIM::Algebraic::Octonion o1(1,2,3,4,5,6,7,8);
    XINIM::Algebraic::Octonion o2(8,7,6,5,4,3,2,1);

    // Addition
    XINIM::Algebraic::Octonion o_add = o1 + o2;
    assert(approx_equal_oct(o_add, XINIM::Algebraic::Octonion(9,9,9,9,9,9,9,9)));

    // Subtraction
    XINIM::Algebraic::Octonion o_sub = o1 - o2;
    assert(approx_equal_oct(o_sub, XINIM::Algebraic::Octonion(-7,-5,-3,-1,1,3,5,7)));

    // Scalar Mult/Div
    assert(approx_equal_oct(o1 * 2.0, XINIM::Algebraic::Octonion(2,4,6,8,10,12,14,16)));
    assert(approx_equal_oct(2.0 * o1, XINIM::Algebraic::Octonion(2,4,6,8,10,12,14,16)));
    assert(approx_equal_oct(o1 / 2.0, XINIM::Algebraic::Octonion(0.5,1,1.5,2,2.5,3,3.5,4)));
    assert((o1 / 0.0).is_zero()); // Division by zero handled

    // Cayley-Dickson Product (a,b)(c,d) = (ac - d*conj(b), da + conj(c)*b)
    // Let o1 = (q1a, q1b) and o2 = (q2a, q2b)
    XINIM::Algebraic::Quaternion q1a = o1.get_q1(); // (1,2,3,4)
    XINIM::Algebraic::Quaternion q1b = o1.get_q2(); // (5,6,7,8)
    XINIM::Algebraic::Quaternion q2a = o2.get_q1(); // (8,7,6,5)
    XINIM::Algebraic::Quaternion q2b = o2.get_q2(); // (4,3,2,1)

    XINIM::Algebraic::Quaternion res_q1 = (q1a * q2a) - (q2b * q1b.conjugate());
    XINIM::Algebraic::Quaternion res_q2 = (q2b * q1a) + (q2a.conjugate() * q1b);
    XINIM::Algebraic::Octonion expected_prod(res_q1, res_q2);

    XINIM::Algebraic::Octonion o_mul = o1 * o2;
    // std::cout << "Calculated product: " << o_mul << std::endl;
    // std::cout << "Expected product: " << expected_prod << std::endl;
    assert(approx_equal_oct(o_mul, expected_prod));

    std::cout << "test_octonion_arithmetic PASSED.\n";
}

void test_octonion_operations() {
    std::cout << "Running test_octonion_operations...\n";
    XINIM::Algebraic::Octonion o(1,2,3,4,5,6,7,8);

    // Conjugate
    XINIM::Algebraic::Octonion o_conj = o.conjugate();
    assert(approx_equal_oct(o_conj, XINIM::Algebraic::Octonion(1,-2,-3,-4,-5,-6,-7,-8)));

    // Norm_sq: 1+4+9+16+25+36+49+64 = 204
    assert(approx_equal_double(o.norm_sq(), 204.0));
    assert(approx_equal_double(o.norm(), std::sqrt(204.0)));

    // Inverse
    XINIM::Algebraic::Octonion o_inv = o.inverse();
    XINIM::Algebraic::Octonion expected_inv = o.conjugate() / o.norm_sq();
    assert(approx_equal_oct(o_inv, expected_inv));

    // o * o.inverse() should be identity (within tolerance due to non-associativity effects on precision)
    XINIM::Algebraic::Octonion o_identity = o * o_inv;
    // std::cout << "o * o_inv: " << o_identity << std::endl;
    assert(approx_equal_oct(o_identity, XINIM::Algebraic::Octonion::identity(), XINIM::Algebraic::OCTONION_EPSILON * 100));

    // Inverse of zero
    assert(XINIM::Algebraic::Octonion::zero().inverse().is_zero());

    // Normalize
    XINIM::Algebraic::Octonion o_normalized = o.normalized();
    assert(o_normalized.is_unit(XINIM::Algebraic::OCTONION_EPSILON * 100)); // Higher tolerance for normalized unit check
    XINIM::Algebraic::Octonion temp_o = o;
    temp_o.normalize();
    assert(temp_o.is_unit(XINIM::Algebraic::OCTONION_EPSILON * 100));

    // Normalize zero
    XINIM::Algebraic::Octonion o_zero_norm = XINIM::Algebraic::Octonion::zero();
    o_zero_norm.normalize();
    assert(o_zero_norm.is_zero());

    // is_unit / is_zero
    assert(!o.is_unit());
    assert(!o.is_zero());
    assert(XINIM::Algebraic::Octonion::identity().is_unit());
    assert(XINIM::Algebraic::Octonion::zero().is_zero());

    std::cout << "test_octonion_operations PASSED.\n";
}

// Test non-associativity: (e1*e2)*e4 != e1*(e2*e4)
// e1*e2 = e3
// e3*e4 = e7
// e2*e4 = e6
// e1*e6 = -e7
void test_octonion_non_associativity() {
    std::cout << "Running test_octonion_non_associativity...\n";
    XINIM::Algebraic::Octonion e1(0,1,0,0,0,0,0,0);
    XINIM::Algebraic::Octonion e2(0,0,1,0,0,0,0,0);
    XINIM::Algebraic::Octonion e4(0,0,0,0,1,0,0,0);

    XINIM::Algebraic::Octonion e3 = e1 * e2; // Should be e3 (0,0,0,1,0,0,0,0)
    // std::cout << "e1*e2 = " << e3 << std::endl;
    assert(approx_equal_oct(e3, XINIM::Algebraic::Octonion(0,0,0,1,0,0,0,0)));

    XINIM::Algebraic::Octonion res1 = (e1 * e2) * e4; // e3 * e4 = e7
    XINIM::Algebraic::Octonion e7(0,0,0,0,0,0,0,1);
    // std::cout << "(e1*e2)*e4 = " << res1 << std::endl;
    assert(approx_equal_oct(res1, e7));

    XINIM::Algebraic::Octonion e6 = e2 * e4; // Should be e6 (0,0,0,0,0,0,1,0)
    // std::cout << "e2*e4 = " << e6 << std::endl;
    assert(approx_equal_oct(e6, XINIM::Algebraic::Octonion(0,0,0,0,0,0,1,0)));

    XINIM::Algebraic::Octonion res2 = e1 * (e2 * e4); // e1 * e6 = -e7
    XINIM::Algebraic::Octonion neg_e7(0,0,0,0,0,0,0,-1);
    // std::cout << "e1*(e2*e4) = " << res2 << std::endl;
    assert(approx_equal_oct(res2, neg_e7));

    assert(!approx_equal_oct(res1, res2));
    std::cout << "test_octonion_non_associativity PASSED.\n";
}


int main() {
    std::cout << "Starting Octonion Tests...\n";

    test_octonion_constructors();
    test_octonion_arithmetic();
    test_octonion_operations();
    test_octonion_non_associativity();

    std::cout << "All Octonion Tests PASSED.\n";
    return 0;
}
